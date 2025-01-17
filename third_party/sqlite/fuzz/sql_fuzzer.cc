// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string>
#include <vector>

#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/sqlite/fuzz/sql_query_grammar.pb.h"
#include "third_party/sqlite/fuzz/sql_query_proto_to_string.h"
#include "third_party/sqlite/fuzz/sql_run_queries.h"

using namespace sql_query_grammar;

// TODO(mpdenton) Fuzzing tasks
// 5. Definitely fix a lot of the syntax errors that SQlite spits out
// 12. CORPUS Indexes on expressions (https://www.sqlite.org/expridx.html) and
// other places using functions on columns???
// 17. Generate a nice big random, well-formed corpus.
// 18. Possibly very difficult for fuzzer to find certain areas of code, because
// some protobufs need to be mutated together. For example, an index on an
// expression is useless to change, if you don't change the SELECTs that use
// that expression. May need to create a mechanism for the protobufs to
// "register" (in the c++ fuzzer) expressions being used for certain purposes,
// and then protobufs can simple reference those expressions later (similarly to
// columns or tables, with just an index). This should be added if coverage
// shows it is the case.

// FIXME in the future
// 1. Rest of the pragmas
// 2. Make sure defensive config is off
// 3. Fuzz the recover extension from the third patch
// 5. Temp-file database, for better fuzzing of VACUUM and journalling.

DEFINE_BINARY_PROTO_FUZZER(const SQLQueries& sql_queries) {
  std::vector<std::string> queries = sql_fuzzer::SQLQueriesToVec(sql_queries);

  if (getenv("LPM_DUMP_NATIVE_INPUT") && queries.size() != 0) {
    std::cout << "_________________________" << std::endl;
    for (std::string query : queries) {
      if (query == ";")
        continue;
      std::cout << query << std::endl;
    }
    std::cout << "------------------------" << std::endl;
  }

  sql_fuzzer::RunSqlQueries(queries);
}
