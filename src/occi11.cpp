#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "occi11.h"
#if GCC_VERSION >= 40902 // 4.9.2
#include <regex>
#endif
#include <iostream>
#include <thread>
#include <sstream>

using namespace oracle::occi;

#ifdef DEBUG
constexpr bool debug = true;
#else
constexpr bool debug = false;
#endif

#define MAXTABLES 20

const std::chrono::seconds occi11::retry_delay = std::chrono::seconds(30);

occi11::occi11(const std::string &user, const std::string &password, const std::string &connection_string) :
		oUser(user), oPassword(password), oConnection(connection_string) {
}

occi11::~occi11() {
	rollback();

	std::lock_guard < std::mutex > lock(mx);
	try {

		if (env) {
			if (conn)
				env->terminateConnection(conn);
			Environment::terminateEnvironment(env);
		}

	} catch (SQLException& e) {
		std::cerr << "failed to close database connection: " << make_exception_string(e) << std::endl;
	}

#ifdef FAST_DEBUG
	assert(!statements.size());
#endif
}

void occi11::connect(bool retry) {
	std::lock_guard < std::mutex > lock(mx);
	if (!conn)
		reconnect(retry);
}

void occi11::reconnect(bool retry) {
	conn = nullptr; // simply nullify it because it is a lost pointer
	if (env) {
		Environment::terminateEnvironment(env);
		env = nullptr;
	}
	statements.clear();
	resultsets.clear();

	while (!conn) {
		try {
			if (!env)
				env = Environment::createEnvironment("UTF8", "UTF8", Environment::THREADED_MUTEXED);

			conn = env->createConnection(oUser, oPassword, oConnection);
			if (conn) {
				conn->setStmtCacheSize(MAXTABLES);
				break;
			}

			const char* err = "failed to connect to database";
			std::cerr << err << std::endl;
			if (!retry)
				throw std::runtime_error(err);

		} catch (SQLException& e) {
			std::stringstream err;
			err << "failed to connect to database: " << make_exception_string(e);
			std::cerr << err.str() << std::endl;
			if (!retry)
				throw std::runtime_error(err.str());
		}

		std::this_thread::sleep_for(retry_delay);
	}
}

void occi11::handle_exception(SQLException& e, Statement *stmt) {
	switch (e.getErrorCode()) {
	case 3114:	// ORA-03114: not connected to ORACLE
	case 3113:	// ORA-03113: end-of-file on communication channel
	case 12537:	// ORA-12537: TNS:connection closed
	case 12541:	// ORA-12541: TNS:no listener
	case 12514:	// ORA-12514: TNS:listener does not currently know of service requested in connect descriptor
	case 1012:	// ORA-01012: not logged on
	case 28:	// ORA-00028: your session has been killed
	case 3135:	// ORA-03135: connection lost contact
	{
		std::lock_guard < std::mutex > lock(mx);
		if (!stmt || statements.find(stmt) != statements.end())
			reconnect();
	}
		// no break

	default:
		throw e;
	}
}

Statement::Status occi11::execute(Statement *stmt) { // @suppress("No return")
	try {
		std::lock_guard < std::mutex > lock(mx);
		return stmt->execute();

	} catch (SQLException& e) {
		handle_exception(e, stmt); // throws
	}
}

ResultSet* occi11::executeQuery(Statement *stmt) { // @suppress("No return")
	try {
		std::lock_guard < std::mutex > lock(mx);
		auto resultset = stmt->executeQuery();
		resultsets.insert(resultset);
		return resultset;

	} catch (SQLException& e) {
		handle_exception(e, stmt); // throws
	}
}

unsigned int occi11::executeUpdate(Statement *stmt) { // @suppress("No return")
	try {
		std::lock_guard < std::mutex > lock(mx);
		return stmt->executeUpdate();

	} catch (SQLException& e) {
		handle_exception(e, stmt); // throws
	}
}

Statement::Status occi11::execute(const std::string& str) {
	if (debug && str.find(":1") == std::string::npos)
		std::cout << str << std::endl;

	Statement* stmt = nullptr;

	try {
		stmt = createStatement(str);
		auto status = execute(stmt);
		terminateStatement(stmt);
		return status;

	} catch (SQLException& e) {
		terminateStatement(stmt);
		throw e;
	}
}

void occi11::executeQuery(const std::string& str, on_result_func on_result) {
	if (debug && str.find(":1") == std::string::npos)
		std::cout << str << std::endl;

	Statement* stmt = nullptr;

	try {
		stmt = createStatement(str);
		auto resultset = executeQuery(stmt);
		while (on_result(this, resultset))
			;
		closeResultSet(resultset);

	} catch (SQLException& e) {
		terminateStatement(stmt);
		throw e;
	}
}

unsigned int occi11::executeUpdate(const std::string& str) {
	if (debug && str.find(":1") == std::string::npos)
		std::cout << str << std::endl;

	Statement* stmt = nullptr;

	try {
		stmt = createStatement(str);
		auto result = executeUpdate(stmt);
		terminateStatement(stmt);
		return result;

	} catch (SQLException& e) {
		terminateStatement(stmt);
		throw e;
	}
}

std::string occi11::make_exception_string(const SQLException& ex) {
	return ex.getMessage(); // boost::replace_all_copy(ex.getMessage(), "\n", "  ");
}

std::string& occi11::make_simple_error_message(std::string& t) {
#if !DEBUG && GCC_VERSION >= 40902 // 4.9.2
	if(t.length()) {
		// trim oracle-related details from text
		std::smatch m;
		if (std::regex_search(t, m, std::regex("ORA-\\d*: (.*?)(  |$)")) && m.size() > 1) {
			t = m[1];
		}
	}
#endif

	return t;
}

void occi11::commit() {
	try {
		std::lock_guard < std::mutex > lock(mx);
		if (conn)
			conn->commit();

	} catch (SQLException& e) {
		handle_exception(e);
	}
}

void occi11::rollback() {
	try {
		std::lock_guard < std::mutex > lock(mx);
		if (conn)
			conn->rollback();

	} catch (SQLException& e) {
		handle_exception(e);
	}
}

Statement* occi11::createStatement(const std::string& str) { // @suppress("No return")
	try {
		std::lock_guard < std::mutex > lock(mx);
		auto stmt = conn->createStatement(str);
		statements.insert(stmt);
		return stmt;

	} catch (SQLException& e) {
		handle_exception(e);
	}
}

void occi11::terminateStatement(Statement* stmt) {
	if (stmt) {
		try {
			std::lock_guard < std::mutex > lock(mx);
			auto it = statements.find(stmt);
			if (it != statements.end()) {
				conn->terminateStatement(stmt);
				statements.erase(it);
			}

		} catch (SQLException& e) {
			std::cerr << "failed to terminate statement: " << make_exception_string(e) << std::endl;
		}
	}
}

Statement::Status occi11::ensureExecute(const std::string& str, on_error_func on_error) {
	return ensureExecute(str, nullptr, on_error);
}

Statement::Status occi11::ensureExecute(const std::string& str, on_create_statement_func on_create_statement, on_error_func on_error, on_batch_error_func on_batch_error) {
	if (debug && str.find(":1") == std::string::npos)
		std::cout << str << std::endl;

	Statement::Status status = (Statement::Status) - 1;

	while (status == (Statement::Status) - 1) {
		Statement *stmt = nullptr;

		try {
			stmt = createStatement(str);
			stmt->setAutoCommit(true);

			if (on_create_statement)
				on_create_statement(stmt);

			status = execute(stmt);

		} catch (BatchSQLException& e) {
			if (on_batch_error)
				on_batch_error(e);

			std::this_thread::sleep_for(retry_delay);

		} catch (SQLException &e) {
			if (on_error)
				on_error(e);

			std::this_thread::sleep_for(retry_delay);
		}

		terminateStatement(stmt);
	}

	return status;
}

unsigned int occi11::ensureExecuteUpdate(const std::string& str, on_error_func on_error) {
	if (debug && str.find(":1") == std::string::npos)
		std::cout << str << std::endl;

	unsigned int result = std::numeric_limits<unsigned int>::max();

	while (result == std::numeric_limits<unsigned int>::max()) {
		Statement *stmt = nullptr;

		try {
			stmt = createStatement(str);
			stmt->setAutoCommit(true);
			result = executeUpdate(stmt);

		} catch (SQLException &e) {
			if (on_error) {
				try {
					on_error(e);

				} catch (const std::exception& e) {
					std::cerr << "internal error: " << e.what() << std::endl;
				}
			}

			std::this_thread::sleep_for(retry_delay);
		}

		terminateStatement(stmt);
	}

	return result;
}

void occi11::ensureExecuteQuery(const std::string& str, on_result_func on_result, on_error_func on_error) {
	if (debug && str.find(":1") == std::string::npos)
		std::cout << str << std::endl;

	int counter = 0; // counts number of rows processed before failure

	while (true) {
		Statement *stmt = nullptr;

		try {
			stmt = createStatement(str);
			auto resultset = executeQuery(stmt);

			// bypass previously processed rows
			bool rows_available = true;
			for (int i = 0; i < counter && rows_available; i++)
				if (!next(resultset))
					rows_available = false;

			while (rows_available && on_result(this, resultset))
				counter++;

			closeResultSet(resultset);
			return;

		} catch (SQLException &e) {
			if (on_error)
				on_error(e);

			terminateStatement(stmt);
			std::this_thread::sleep_for(retry_delay);

		} catch (const std::exception& e) {
			const std::string w = e.what();
			if (w.length())
				std::cerr << w << std::endl;

			terminateStatement(stmt);
			std::this_thread::sleep_for(retry_delay);

		} catch (...) {
			terminateStatement(stmt);
			std::this_thread::sleep_for(retry_delay);
		}
	}
}

ResultSet::Status occi11::next(ResultSet* resultset) {
	std::lock_guard < std::mutex > lock(mx);
	if (resultsets.find(resultset) == resultsets.end())
		throw std::runtime_error("");

	return resultset->next();
}

void occi11::closeResultSet(ResultSet* result) {
	if (result) {
		Statement* stmt = nullptr;

		try {
			std::lock_guard < std::mutex > lock(mx);
			auto it = resultsets.find(result);
			if (it == resultsets.end())
				return;

			stmt = result->getStatement();
			if (stmt)
				stmt->closeResultSet(result);

			resultsets.erase(it);

		} catch (SQLException& e) {
			std::cerr << "failed to close result set: " << make_exception_string(e) << std::endl;
		}

		terminateStatement(stmt);
	}
}
