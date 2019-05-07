#ifndef __OCCI11_H__
#define __OCCI11_H__

#include <occi.h>
#include <string>
#include <vector>
#include <set>
#include <mutex>
#include <functional>
#include <chrono>

class occi11 {
	std::string oUser, oPassword, oConnection;

	oracle::occi::Environment *env = nullptr;
	oracle::occi::Connection *conn = nullptr;
	std::mutex mx; // handles mutually exclusive access to connection

	std::set<oracle::occi::Statement*> statements; // list of statements owned by current connection
	std::set<oracle::occi::ResultSet*> resultsets;

	void reconnect(bool retry = true); // recreates connection if previous one is not usable any more
	void handle_exception(oracle::occi::SQLException& e, oracle::occi::Statement *stmt = nullptr);

	oracle::occi::Statement::Status execute(oracle::occi::Statement *stmt);
	oracle::occi::ResultSet* executeQuery(oracle::occi::Statement *stmt);
	unsigned int executeUpdate(oracle::occi::Statement *stmt);

	oracle::occi::Statement* createStatement(const std::string& str);
	void terminateStatement(oracle::occi::Statement* stmt);

	void closeResultSet(oracle::occi::ResultSet* result);

public:
	occi11(const std::string &user, const std::string &password, const std::string &connection_string);
	virtual ~occi11();

	void connect(bool retry = true);

	void commit();
	void rollback();

	using on_result_func = std::function<bool(occi11*, oracle::occi::ResultSet*)>; // return false to stop querying next result

	oracle::occi::Statement::Status execute(const std::string& stmt); // a wrapper around oracle::occi::execute that detects connection error and does a reconnect attempt; it throws any exception that is thrown by oracle
	void executeQuery(const std::string& stmt, on_result_func on_result); // a wrapper around oracle::occi::executeQuery that detects connection error and does a reconnect attempt; it throws any exception that is thrown by oracle
	unsigned int executeUpdate(const std::string& stmt); // a wrapper around oracle::occi::executeUpdate that detects connection error and does a reconnect attempt; it throws any exception that is thrown by oracle

	using on_success_func = std::function<void(const oracle::occi::Statement*)>;
	using on_error_func = std::function<void(const oracle::occi::SQLException&)>;
	using on_batch_error_func = std::function<void(const oracle::occi::BatchSQLException&)>;
	using on_create_statement_func = std::function<void(oracle::occi::Statement*)>;

	oracle::occi::Statement::Status ensureExecute(const std::string& stmt, on_create_statement_func on_create_statement, on_error_func on_error = nullptr, on_batch_error_func on_batch_error = nullptr); // blocks until successful execution of the statement; if a connection-related error occurs, it will re-try after reconnect
	oracle::occi::Statement::Status ensureExecute(const std::string& stmt, on_error_func on_error = nullptr); // blocks until successful execution of the statement; if a connection-related error occurs, it will re-try after reconnect
	unsigned int ensureExecuteUpdate(const std::string& stmt, on_error_func on_error = nullptr); // blocks until successful execution of the statement; if a connection-related error occurs, it will re-try after reconnect
	void ensureExecuteQuery(const std::string& stmt, on_result_func on_result, on_error_func on_error = nullptr); // blocks until successful execution of the statement; if a connection-related error occurs, it will re-try after reconnect

	oracle::occi::ResultSet::Status next(oracle::occi::ResultSet* resultset); // a wrapper around oracle::occi::Statement::next which detects connection error and does a reconnect attempt; it throws any exception that is thrown by oracle; this method is supposed to be called within an on_result_func handler

	static std::string make_exception_string(const ::oracle::occi::SQLException& e);
	static std::string& make_simple_error_message(std::string& str);

	static const std::chrono::seconds retry_delay; // retry delay to execute statement again after failure
};

#endif /* __OCCI11_H__ */
