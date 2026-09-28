// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include <algorithm>
#include <cstdlib>
#include <deque>
// cpplint's approved header list stops at C++14 and this project is C++20, so
// <filesystem> has to say so itself.
#include <filesystem>  // NOLINT(build/c++17)
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include "benchmark/benchmark.h"
#include "fpag/base/numeric.h"
#include "fpag/io/io_util.h"
#include "fpag/str/string_interner.h"

namespace str {

namespace {

// The interner's stream, taken from real code rather than invented.
//
// A compiler front end interns the identifiers of the translation unit in front
// of it, over and over, and the names repeat because the same header is read by
// every unit. That gives an interner two properties a synthetic stream does
// not: the working set at any moment is small while the table is large, and a
// lookup is a memory access rather than a comparison. Both come out of the
// corpus for free, because the corpus is this repository's own sources lexed in
// order: the identifier frequency distribution is the real one (a few hundred
// names carry most of the occurrences), and walking the file list in parallel
// is walking translation units in parallel.
//
// The roots are the source tree's include/ and src/, plus third_party/ when a
// dependency is vendored there, plus whatever FPAG_BENCH_CORPUS names (a list
// separated by ';'), which is where a fetched fmt or catch2 source tree goes.

constexpr u64 HOT_PERCENT = 1;
constexpr usize LOOKUPS_PER_INSERT = 5;
constexpr usize REAL_NAME_PER_NEW = 5;
// How many times the corpus is walked before the invented names run out, which
// is what decides how much of the mixed phase is a first insert.
constexpr usize ROUNDS_PER_CORPUS = 8;

struct Corpus {
  // A deque, not a vector: the identifier views point into these strings, and a
  // short string's bytes live inside the object, so a vector's reallocation
  // would leave the views of every short file dangling.
  std::deque<std::string> texts;
  std::vector<std::vector<std::string_view>> identifiers;
  std::vector<std::string_view> hot;
  std::vector<std::string_view> cold;
  // A deque for the same reason as texts: the views point into these strings,
  // and a vector of them would move them under the views.
  std::deque<std::string> new_name_texts;
  std::vector<std::string_view> new_names;
  u64 occurrences = 0;
  u32 distinct = 0;
};

bool is_source_extension(std::string_view extension) {
  return extension == ".h" || extension == ".cc" || extension == ".hpp" ||
         extension == ".ipp";
}

void collect_files(const std::filesystem::path& root,
                   std::vector<std::filesystem::path>* out) {
  std::error_code error;
  if (!std::filesystem::is_directory(root, error)) {
    return;
  }
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::recursive_directory_iterator(root, error)) {
    if (entry.is_regular_file(error) &&
        is_source_extension(entry.path().extension().string())) {
      out->push_back(entry.path());
    }
  }
}

constexpr bool is_identifier_start(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

constexpr bool is_identifier_char(char c) {
  return is_identifier_start(c) || (c >= '0' && c <= '9');
}

// Appends the identifiers in @p text, in order, skipping the ones a front end
// never sees: the English in comments, the text in literals, and the body of a
// raw string. A regular expression cannot do this without re-scanning what it
// has just matched, so it is a state machine.
void lex_identifiers(std::string_view text,
                     std::vector<std::string_view>* out) {
  enum class State : u8 { Code, LineComment, BlockComment, String, Char };
  State state = State::Code;
  usize index = 0;
  while (index < text.size()) {
    const char c = text[index];
    switch (state) {
      case State::Code: {
        const bool has_next = index + 1 < text.size();
        if (c == '/' && has_next && text[index + 1] == '/') {
          state = State::LineComment;
          index += 2;
        } else if (c == '/' && has_next && text[index + 1] == '*') {
          state = State::BlockComment;
          index += 2;
        } else if (c == 'R' && has_next && text[index + 1] == '"') {
          // A raw string ends at the first )" after its opening, which is close
          // enough for source that lexes cleanly.
          const usize end = text.find(")\"", index + 2);
          index = end == std::string_view::npos ? text.size() : end + 2;
        } else if (c == '"') {
          state = State::String;
          ++index;
        } else if (c == '\'') {
          state = State::Char;
          ++index;
        } else if (is_identifier_start(c)) {
          usize end = index;
          while (end < text.size() && is_identifier_char(text[end])) {
            ++end;
          }
          out->emplace_back(text.data() + index, end - index);
          index = end;
        } else {
          ++index;
        }
        break;
      }
      case State::LineComment:
        state = c == '\n' ? State::Code : State::LineComment;
        ++index;
        break;
      case State::BlockComment:
        if (c == '*' && index + 1 < text.size() && text[index + 1] == '/') {
          state = State::Code;
          index += 2;
        } else {
          ++index;
        }
        break;
      case State::String:
      case State::Char: {
        if (c == '\\') {
          index += 2;
        } else if ((state == State::String && c == '"') ||
                   (state == State::Char && c == '\'')) {
          state = State::Code;
          ++index;
        } else {
          ++index;
        }
        break;
      }
    }
  }
}

std::vector<std::filesystem::path> corpus_roots() {
  const std::filesystem::path source(FPAG_BENCH_SOURCE_DIR);
  std::vector<std::filesystem::path> roots = {
      source / "include", source / "src", source / "third_party"};
  const char* const extra = std::getenv("FPAG_BENCH_CORPUS");
  if (extra == nullptr) {
    return roots;
  }
  const std::string_view list(extra);
  usize start = 0;
  while (start <= list.size()) {
    const usize end = list.find(';', start);
    const std::string_view root =
        list.substr(start, end == std::string_view::npos ? list.size() - start
                                                         : end - start);
    if (!root.empty()) {
      roots.emplace_back(root);
    }
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return roots;
}

Corpus load_corpus() {
  Corpus corpus;
  std::vector<std::filesystem::path> files;
  for (const std::filesystem::path& root : corpus_roots()) {
    collect_files(root, &files);
  }
  // A stable order, so two runs walk the same stream in the same sequence and
  // the hot set is the same set.
  std::sort(files.begin(), files.end());

  std::unordered_map<std::string_view, u32> frequency;
  for (const std::filesystem::path& path : files) {
    corpus.texts.push_back(io::read_file(path.string()));
    std::vector<std::string_view> identifiers;
    lex_identifiers(corpus.texts.back(), &identifiers);
    corpus.occurrences += identifiers.size();
    for (const std::string_view identifier : identifiers) {
      ++frequency[identifier];
    }
    corpus.identifiers.push_back(std::move(identifiers));
  }

  std::vector<std::string_view> by_frequency;
  by_frequency.reserve(frequency.size());
  for (const auto& entry : frequency) {
    by_frequency.push_back(entry.first);
  }
  // Most frequent first, with the identifier as the tiebreak, so the hot and
  // cold split is a property of the corpus rather than of the run.
  std::sort(by_frequency.begin(), by_frequency.end(),
            [&frequency](std::string_view lhs, std::string_view rhs) {
              const u32 lhs_count = frequency[lhs];
              const u32 rhs_count = frequency[rhs];
              if (lhs_count != rhs_count) {
                return lhs_count > rhs_count;
              }
              return lhs < rhs;
            });

  corpus.distinct = static_cast<u32>(by_frequency.size());
  const usize hot_count = std::max<usize>(
      1, static_cast<usize>(corpus.distinct) * HOT_PERCENT / 100);
  corpus.hot.reserve(hot_count);
  corpus.cold.reserve(by_frequency.size() - hot_count);
  for (usize index = 0; index < by_frequency.size(); ++index) {
    if (index < hot_count) {
      corpus.hot.push_back(by_frequency[index]);
    } else {
      corpus.cold.push_back(by_frequency[index]);
    }
  }

  // The names a thread has not interned yet. The corpus is finite, so a first
  // insert needs a name outside it: a suffix keeps the content realistic (a
  // real name with a number on the end) and every one of them is a miss.
  const u32 new_count = std::max<u32>(1, corpus.distinct / 4);
  corpus.new_name_texts.resize(new_count);
  for (u32 index = 0; index < new_count; ++index) {
    std::string& name = corpus.new_name_texts[index];
    name.assign(by_frequency[index % by_frequency.size()]);
    name += "_n";
    name += std::to_string(index);
    corpus.new_names.emplace_back(name);
  }
  return corpus;
}

const Corpus& stream() {
  static const Corpus corpus = load_corpus();
  return corpus;
}

// How big the table has to be for what the benchmark interns: every distinct
// identifier in the corpus, plus the names the mixed phase invents, plus half
// again as headroom. The headroom is not decoration. Today's interner takes a
// slot count, insists on a power of two, and does not grow, so a table that
// runs out of slots is a fatal check; and linear probing over a table that is
// four fifths full walks a dozen slots per operation, which would make this a
// benchmark of a nearly full table rather than of the table.
usize table_capacity() {
  const Corpus& corpus = stream();
  const usize interned = static_cast<usize>(corpus.distinct) +
                         corpus.new_names.size() + corpus.distinct / 2;
  usize slots = 1;
  while (slots < interned) {
    slots <<= 1;
  }
  return slots;
}

// One interner per benchmark, shared by that benchmark's threads: a front end
// parallelises over translation units but interns into one table, and a
// per-thread table would measure something no caller has.
StringInterner& shared_interner() {
  static StringInterner interner(table_capacity());
  static const bool filled = [] {
    for (const auto& file : stream().identifiers) {
      for (const std::string_view identifier : file) {
        benchmark::DoNotOptimize(interner.intern(identifier));
      }
    }
    return true;
  }();
  (void)filled;
  return interner;
}

void report_corpus(benchmark::State& state) {
  if (state.thread_index() != 0) {
    return;
  }
  const Corpus& corpus = stream();
  state.counters["distinct"] = static_cast<double>(corpus.distinct);
  state.counters["occurrences"] = static_cast<double>(corpus.occurrences);
  state.counters["hot"] = static_cast<double>(corpus.hot.size());
}

// NOLINTBEGIN(clang-analyzer-deadcode.DeadStores)

// Interns every identifier occurrence in the corpus, which is what filling a
// fresh interner costs: every operation is a first insert, so every one of them
// takes the miss path and appends to the pool.
void interner_fill(benchmark::State& state) {
  const Corpus& corpus = stream();
  StringInterner interner(table_capacity());
  u64 interned = 0;

  for (auto _ : state) {
    for (i32 file = state.thread_index();
         file < static_cast<i32>(corpus.identifiers.size());
         file += state.threads()) {
      for (const std::string_view identifier :
           corpus.identifiers[static_cast<usize>(file)]) {
        benchmark::DoNotOptimize(interner.intern(identifier));
        ++interned;
      }
    }
    state.SetItemsProcessed(static_cast<i64>(interned));
  }
  if (state.thread_index() == 0) {
    state.counters["pool_bytes"] = static_cast<double>(interner.size());
    state.counters["strings"] = static_cast<double>(interner.string_count());
  }
  report_corpus(state);
}
BENCHMARK(interner_fill)->Threads(1)->Threads(4)->Threads(8);

// The read side over a filled table: one intern per identifier occurrence, in
// corpus order, which is a front end reading a translation unit it has already
// interned. Every operation is a hit, so the pool does not grow.
void interner_lookup_all(benchmark::State& state) {
  const Corpus& corpus = stream();
  StringInterner& interner = shared_interner();
  u64 looked_up = 0;

  for (auto _ : state) {
    for (i32 file = state.thread_index();
         file < static_cast<i32>(corpus.identifiers.size());
         file += state.threads()) {
      for (const std::string_view identifier :
           corpus.identifiers[static_cast<usize>(file)]) {
        benchmark::DoNotOptimize(interner.intern(identifier));
        ++looked_up;
      }
    }
    state.SetItemsProcessed(static_cast<i64>(looked_up));
  }
  report_corpus(state);
}
BENCHMARK(interner_lookup_all)->Threads(1)->Threads(4)->Threads(8);

// The same read path over the hottest percent of the corpus. The table is the
// same size; the working set is not, so this is the case a table tuned for a
// DRAM-resident table is not the case for.
void interner_lookup_hot(benchmark::State& state) {
  const Corpus& corpus = stream();
  StringInterner& interner = shared_interner();
  const u32 threads = static_cast<u32>(state.threads());
  const usize repeats = corpus.hot.size() / threads + 1;
  u64 looked_up = 0;

  for (auto _ : state) {
    for (u32 index = static_cast<u32>(state.thread_index());
         index < static_cast<u32>(corpus.hot.size()); index += threads) {
      for (usize repeat = 0; repeat < repeats; ++repeat) {
        benchmark::DoNotOptimize(interner.intern(corpus.hot[index]));
        ++looked_up;
      }
    }
    state.SetItemsProcessed(static_cast<i64>(looked_up));
  }
  report_corpus(state);
}
BENCHMARK(interner_lookup_hot)->Threads(1)->Threads(4)->Threads(8);

// The shape a real interner mostly sees: a stream that is mostly hits, walking
// the same sources again, with one operation in five interning a name this
// thread has not seen. One of those five inserts a name that is already
// interned, which is what a build does when every translation unit reads the
// same header: the pool takes a copy no id ever points at, and the counters
// below are how much of the pool that is.
void interner_mixed(benchmark::State& state) {
  const Corpus& corpus = stream();
  StringInterner& interner = shared_interner();
  // Every thread walks the same insert order, and each repetition interns a
  // slice of names no earlier repetition has seen, so an inserted name is new
  // to the table and is being reached for by every other thread at the same
  // time: the case where two of them want one name, which is what a build does
  // when every translation unit reads the same header. How often a thread loses
  // that race and pays the pool for a copy no id names is timing, not
  // structure, so the strings counter below is the measurement: on this host it
  // comes out at distinct + new_names, because the threads arrive far enough
  // apart that the winner has already published.
  const usize slice = corpus.new_names.size() / ROUNDS_PER_CORPUS + 1;
  u64 operations = 0;
  u64 inserts = 0;
  usize round = 0;

  for (auto _ : state) {
    const usize base = round * slice;
    ++round;
    usize next_real = static_cast<usize>(state.thread_index());
    for (i32 file = state.thread_index();
         file < static_cast<i32>(corpus.identifiers.size());
         file += state.threads()) {
      for (const std::string_view identifier :
           corpus.identifiers[static_cast<usize>(file)]) {
        if (operations % LOOKUPS_PER_INSERT == 0) {
          const std::string_view name =
              inserts % REAL_NAME_PER_NEW == 0
                  ? corpus.cold[next_real++ % corpus.cold.size()]
                  : corpus.new_names[(base + inserts / REAL_NAME_PER_NEW) %
                                     corpus.new_names.size()];
          benchmark::DoNotOptimize(interner.intern(name));
          ++inserts;
        } else {
          benchmark::DoNotOptimize(interner.intern(identifier));
        }
        ++operations;
      }
    }
    state.SetItemsProcessed(static_cast<i64>(operations));
  }
  if (state.thread_index() == 0) {
    state.counters["pool_bytes"] = static_cast<double>(interner.size());
    state.counters["strings"] = static_cast<double>(interner.string_count());
  }
  report_corpus(state);
}
BENCHMARK(interner_mixed)->Threads(1)->Threads(4)->Threads(8);

// NOLINTEND(clang-analyzer-deadcode.DeadStores)

}  // namespace

}  // namespace str
