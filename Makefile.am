dist_doc_DATA = README

AM_CXXFLAGS = -std=c++11 -I@with_occi@ -D_GLIBCXX_USE_CXX11_ABI=0

bin_PROGRAMS = test
test_SOURCES = src/main.cpp src/occi11.cpp
test_LDFLAGS = -L@with_occilib@
test_LDADD = -lclntsh -locci -lpthread
