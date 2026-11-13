// Responsabilidad: auto-pruebas aisladas con datos sintéticos y un adaptador
// falso. No inicializan el portapapeles, no abren red, no capturan entrada y
// no usan certificados reales. Se ejecutan con --self-test.

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine.hpp"
#include "model.hpp"
#include "protocol.hpp"
#ifdef OIT_HAVE_PORTAL
#include "platform/portal_clipboard.hpp"
#endif

namespace oit {
namespace {

int g_checks = 0;
int g_failures = 0;
std::string g_current;

void check(bool condition, const char* expression, int line) {
  ++g_checks;
  if (!condition) {
    ++g_failures;
    std::cerr << "  FAIL " << g_current << ":" << line << "  " << expression
              << "\n";
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

void run_test(const char* name, void (*test)()) {
  g_current = name;
  const int before = g_failures;
  test();
  std::cout << (g_failures == before ? "PASS " : "FAIL ") << name << "\n";
}

class FakePort final : public ClipboardPort {
 public:
  ClipboardCapabilities capabilities() const override {
    ClipboardCapabilities caps;
    caps.read = true;
    caps.write = true;
    caps.watch = true;
    caps.independent_of_input = true;
    caps.backend = "fake";
    caps.session = "synthetic";
    return caps;
  }

  Status read(ClipboardContent& out) override {
    out = content;
    return {StatusCode::Ok, ""};
  }

  Status write(std::string_view text, std::string_view marker) override {
    ++writes;
    content.text = std::string(text);
    content.marker = std::string(marker);
    return {StatusCode::Ok, ""};
  }

  WaitResult wait_for_change(ClipboardContent&, int, std::string&) override {
    return WaitResult::Timeout;
  }

  void request_stop() override {}

  ClipboardContent content;
  int writes = 0;
};

ClipboardUpdate make_update(std::uint64_t generation, std::uint64_t logical,
                            std::string origin, std::string text) {
  ClipboardUpdate update;
  update.generation = generation;
  update.logical = logical;
  update.origin = std::move(origin);
  update.text = std::move(text);
  return update;
}

struct Harness {
  explicit Harness(std::uint64_t generation = 42,
                   std::string self_id = "aaaa1111") {
    options.self_id = std::move(self_id);
    options.generation = generation;
    options.max_text_bytes = kMaxTextBytes;
    options.echo_window_ms = kEchoWindowMs;
    options.now_ms = [this] { return now; };
    engine = std::make_unique<SyncEngine>(
        &port, options,
        [this](const ClipboardUpdate& update) {
          sent.push_back(update);
          return send_ok;
        },
        [this](const EngineEvent& event) { events.push_back(event); });
  }

  FakePort port;
  std::vector<ClipboardUpdate> sent;
  std::vector<EngineEvent> events;
  std::int64_t now = 1000;
  bool send_ok = true;
  EngineOptions options;
  std::unique_ptr<SyncEngine> engine;
};

void test_text_validation() {
  {
    const Validation validation = validate_text(u8"hola ñ 😀");
    CHECK(validation.valid);
    CHECK(validation.metrics.codepoints == 8);
    CHECK(validation.metrics.lines == 1);
    CHECK(validation.normalized == u8"hola ñ 😀");
    CHECK(validation.status.ok());
  }
  {
    const Validation validation = validate_text("");
    CHECK(!validation.valid);
    CHECK(validation.status.code == StatusCode::InvalidText);
  }
  {
    const std::string with_nul("a\0b", 3);
    CHECK(!validate_text(with_nul).valid);
  }
  {
    const std::string invalid[] = {
        std::string("\xC0\x80", 2),
        std::string("\xED\xA0\x80", 3),
        std::string("\xF0\x9F", 2),
        std::string("\x80", 1),
        std::string("\xFF", 1),
    };
    for (const std::string& text : invalid) {
      CHECK(!validate_text(text).valid);
    }
  }
  {
    std::string at_limit(kMaxTextBytes, 'a');
    CHECK(validate_text(at_limit).valid);
    at_limit.push_back('a');
    const Validation validation = validate_text(at_limit);
    CHECK(!validation.valid);
    CHECK(validation.status.code == StatusCode::TooLarge);
  }
  {
    const Validation validation = validate_text("a\r\nb\rc\nd");
    CHECK(validation.valid);
    CHECK(validation.normalized == "a\nb\rc\nd");
    CHECK(validation.metrics.lines == 3);
  }
  {
    const Validation validation = validate_text(u8"línea\n");
    CHECK(validation.valid);
    CHECK(validation.metrics.codepoints == 6);
    CHECK(validation.metrics.lines == 2);
  }
}

void test_line_endings() {
  CHECK(to_windows_line_endings("a\nb") == "a\r\nb");
  CHECK(to_windows_line_endings("a\r\nb") == "a\r\nb");
  CHECK(to_windows_line_endings("a\rb") == "a\rb");
  CHECK(normalize_line_endings(to_windows_line_endings("a\nb\r\nc")) ==
        "a\nb\nc");
  CHECK(normalize_line_endings("a\rb") == "a\rb");
}

void test_ordering_and_ids() {
  const ClipboardUpdate a = make_update(1, 5, "aaaa", "A");
  const ClipboardUpdate b = make_update(1, 5, "bbbb", "B");
  CHECK(compare_updates(b, a) > 0);
  CHECK(compare_updates(a, b) < 0);
  CHECK(compare_updates(a, a) == 0);
  CHECK(update_is_newer(b, a));
  CHECK(!update_is_newer(a, b));

  const ClipboardUpdate c = make_update(1, 6, "aaaa", "C");
  CHECK(update_is_newer(c, b));

  const ClipboardUpdate other_generation = make_update(9, 5, "bbbb", "B");
  CHECK(compare_updates(other_generation, b) == 0);

  CHECK(valid_endpoint_id("aaaa1111"));
  CHECK(!valid_endpoint_id(""));
  CHECK(!valid_endpoint_id("bad id"));
  CHECK(!valid_endpoint_id(std::string(65, 'a')));

  const std::string first = make_endpoint_id();
  const std::string second = make_endpoint_id();
  CHECK(valid_endpoint_id(first));
  CHECK(valid_endpoint_id(second));
  CHECK(first != second);
  CHECK(make_generation() != 0);
}

void test_fingerprint() {
  CHECK(fingerprint("hola") == fingerprint("hola"));
  CHECK(fingerprint("hola") != fingerprint(u8"holà"));
  CHECK(hex_u64(0) == "0000000000000000");
  CHECK(fingerprint_hex(1) == "0000000000000001");
}

void test_protocol_roundtrip_and_limits() {
  Status error;
  DecodedMessage decoded;

  HelloMessage hello;
  hello.generation = 123;
  hello.endpoint = "aaaa1111";
  hello.can_read = true;
  hello.can_write = true;
  hello.can_watch = false;
  CHECK(decode_message(encode_hello(hello), decoded, error));
  CHECK(decoded.type == MessageType::Hello);
  CHECK(decoded.hello.generation == 123);
  CHECK(decoded.hello.endpoint == "aaaa1111");
  CHECK(decoded.hello.can_read);
  CHECK(!decoded.hello.can_watch);
  CHECK(validate_hello(decoded.hello).ok());

  ClipboardMessage clipboard;
  clipboard.generation = 123;
  clipboard.logical = 7;
  clipboard.origin = "bbbb2222";
  clipboard.text = u8"hola ñ 😀\nlínea 2";
  const Bytes valid = encode_clipboard(clipboard);
  CHECK(decode_message(valid, decoded, error));
  CHECK(decoded.type == MessageType::Clipboard);
  CHECK(decoded.clipboard.text == clipboard.text);
  CHECK(decoded.clipboard.logical == 7);

  AppliedMessage applied;
  applied.generation = 123;
  applied.logical = 7;
  applied.origin = "bbbb2222";
  applied.code = StatusCode::TooLarge;
  applied.detail = "too big";
  CHECK(decode_message(encode_applied(applied), decoded, error));
  CHECK(decoded.applied.code == StatusCode::TooLarge);
  CHECK(validate_applied(decoded.applied).ok());

  const ErrorMessage failure{StatusCode::WrongProtocol, "version"};
  CHECK(decode_message(encode_error(failure), decoded, error));
  CHECK(decoded.error.code == StatusCode::WrongProtocol);

  const ByeMessage bye{"done"};
  CHECK(decode_message(encode_bye(bye), decoded, error));
  CHECK(decoded.bye.reason == "done");

  CHECK(!decode_message(Bytes{}, decoded, error));

  Bytes truncated = valid;
  truncated.pop_back();
  CHECK(!decode_message(truncated, decoded, error));

  Bytes trailing = valid;
  trailing.push_back(0);
  CHECK(!decode_message(trailing, decoded, error));

  Bytes zero_length(5, 0);
  zero_length[4] = 1;
  CHECK(!decode_message(zero_length, decoded, error));

  Bytes unknown = valid;
  unknown[4] = 99;
  CHECK(!decode_message(unknown, decoded, error));
  CHECK(error.code == StatusCode::NotSupported);

  Bytes huge(5, 0);
  huge[1] = 0x10;
  huge[4] = 1;
  CHECK(!decode_message(huge, decoded, error));
  CHECK(error.code == StatusCode::TooLarge);

  HelloMessage wrong_version = hello;
  wrong_version.version = 999;
  CHECK(validate_hello(wrong_version).code == StatusCode::WrongProtocol);

  HelloMessage zero_generation = hello;
  zero_generation.generation = 0;
  CHECK(!validate_hello(zero_generation).ok());

  HelloMessage empty_endpoint = hello;
  empty_endpoint.endpoint.clear();
  CHECK(!validate_hello(empty_endpoint).ok());

  AppliedMessage invalid_applied;
  CHECK(!validate_applied(invalid_applied).ok());
}

void test_engine_local_capture() {
  Harness harness;
  harness.port.content = ClipboardContent{u8"hola ñ 😀", ""};
  harness.engine->on_local_clipboard(harness.port.content);
  CHECK(harness.sent.size() == 1);
  CHECK(harness.sent[0].generation == 42);
  CHECK(harness.sent[0].origin == "aaaa1111");
  CHECK(harness.sent[0].logical == 1);
  CHECK(harness.sent[0].text == u8"hola ñ 😀");
  CHECK(!harness.events.empty());
  CHECK(harness.events.back().kind == EngineEventKind::LocalSent);
  CHECK(harness.events.back().status.ok());

  Harness oversized;
  oversized.engine->on_local_clipboard(
      ClipboardContent{std::string(kMaxTextBytes + 1, 'a'), ""});
  CHECK(oversized.sent.empty());
  CHECK(oversized.events.back().kind == EngineEventKind::LocalRejected);
  CHECK(oversized.events.back().status.code == StatusCode::TooLarge);

  Harness invalid;
  invalid.engine->on_local_clipboard(
      ClipboardContent{std::string("\xC0\x80", 2), ""});
  CHECK(invalid.sent.empty());
  CHECK(invalid.events.back().status.code == StatusCode::InvalidText);

  Harness empty;
  empty.engine->on_local_clipboard(ClipboardContent{"", ""});
  CHECK(empty.sent.empty());
  CHECK(empty.events.back().status.code == StatusCode::InvalidText);

  Harness disconnected;
  disconnected.send_ok = false;
  disconnected.engine->on_local_clipboard(ClipboardContent{"hola", ""});
  CHECK(disconnected.events.back().kind == EngineEventKind::LocalSent);
  CHECK(disconnected.events.back().status.code == StatusCode::IoError);
}

void test_engine_remote_and_echo() {
  Harness harness;
  const ClipboardUpdate update = make_update(42, 5, "bbbb2222", u8"hola\nñ 😀");
  const ApplyResult applied = harness.engine->apply_remote(update);
  CHECK(applied.applied);
  CHECK(applied.status.ok());
  CHECK(harness.port.writes == 1);
  CHECK(harness.port.content.text == u8"hola\nñ 😀");
  CHECK(harness.port.content.marker.rfind("oit1:aaaa1111:", 0) == 0);
  CHECK(harness.engine->is_self_marker(harness.port.content.marker));
  CHECK(harness.engine->is_self_marker("oit1:aaaa1111:any-prefix"));
  CHECK(!harness.engine->is_self_marker("oit1:bbbb2222:1:x:1"));
  CHECK(harness.engine->has_content());
  CHECK(harness.engine->current()->logical == 5);

  const std::size_t sent_before = harness.sent.size();
  harness.engine->on_local_clipboard(harness.port.content);
  CHECK(harness.sent.size() == sent_before);
  CHECK(harness.events.back().kind == EngineEventKind::LocalEchoSuppressed);
  CHECK(harness.events.back().detail.find("marker") != std::string::npos);

  const ClipboardContent without_marker{harness.port.content.text, ""};
  harness.engine->on_local_clipboard(without_marker);
  CHECK(harness.sent.size() == sent_before);
  CHECK(harness.events.back().kind == EngineEventKind::LocalEchoSuppressed);
  CHECK(harness.events.back().detail.find("fingerprint") !=
        std::string::npos);

  harness.now += 5000;
  harness.engine->on_local_clipboard(without_marker);
  CHECK(harness.sent.size() == sent_before + 1);
  CHECK(harness.sent.back().logical == 6);

  const ApplyResult duplicate = harness.engine->apply_remote(update);
  CHECK(duplicate.duplicate);
  CHECK(!duplicate.applied);
  CHECK(harness.port.writes == 1);

  const ApplyResult stale =
      harness.engine->apply_remote(make_update(42, 4, "bbbb2222", "old"));
  CHECK(stale.stale);
  CHECK(!stale.applied);
  CHECK(harness.port.writes == 1);

  const ApplyResult old_generation = harness.engine->apply_remote(
      make_update(41, 99, "bbbb2222", "old generation"));
  CHECK(old_generation.wrong_generation);
  CHECK(!old_generation.applied);
  CHECK(harness.port.writes == 1);
}

void test_engine_rejections() {
  Harness harness;
  CHECK(harness.engine
            ->apply_remote(make_update(42, 5, "bbbb2222",
                                       std::string("\xC0\x80", 2)))
            .status.code == StatusCode::InvalidText);
  CHECK(harness.engine
            ->apply_remote(make_update(
                42, 6, "bbbb2222", std::string(kMaxTextBytes + 1, 'a')))
            .status.code == StatusCode::TooLarge);
  CHECK(harness.engine->apply_remote(make_update(42, 7, "bad id", "x"))
            .status.code == StatusCode::InvalidText);
  CHECK(harness.engine->apply_remote(make_update(42, 0, "bbbb2222", "x"))
            .status.code == StatusCode::InvalidText);
  CHECK(harness.engine->apply_remote(make_update(42, 8, "bbbb2222", ""))
            .status.code == StatusCode::InvalidText);
  CHECK(harness.port.writes == 0);
  CHECK(harness.sent.empty());

  Harness normalized;
  CHECK(normalized.engine->apply_remote(
      make_update(42, 9, "bbbb2222", "a\r\nb"))
            .applied);
  CHECK(normalized.port.content.text == "a\nb");
}

void test_engine_concurrency() {
  const ClipboardUpdate first = make_update(42, 10, "aaaa1111", "A");
  const ClipboardUpdate second = make_update(42, 10, "bbbb2222", "B");

  Harness left;
  CHECK(left.engine->apply_remote(first).applied);
  CHECK(left.engine->apply_remote(second).applied);
  CHECK(left.port.content.text == "B");

  Harness right;
  CHECK(right.engine->apply_remote(second).applied);
  CHECK(right.engine->apply_remote(first).stale);
  CHECK(right.port.content.text == "B");

  Harness logical;
  CHECK(logical.engine->apply_remote(make_update(42, 20, "aaaa1111", "newer"))
            .applied);
  CHECK(logical.engine
            ->apply_remote(make_update(42, 19, "zzzz9999", "older"))
            .stale);
  CHECK(logical.port.content.text == "newer");
}

#ifdef OIT_HAVE_PORTAL
void test_portal_message_helpers() {
  std::string detail;
  const bool ok = run_portal_isolated_checks(detail);
  if (!ok) {
    std::cerr << "  portal detail: " << detail << "\n";
  }
  CHECK(ok);
  CHECK(!detail.empty());
}

void test_portal_pipe_reads() {
  std::string detail;
  const bool ok = run_portal_pipe_isolated_checks(detail);
  if (!ok) {
    std::cerr << "  portal pipe detail: " << detail << "\n";
  }
  CHECK(ok);
  CHECK(!detail.empty());
}
#endif

void test_engine_session_isolation() {
  Harness reconnected(99, "cccc3333");
  CHECK(reconnected.sent.empty());
  CHECK(reconnected.port.writes == 0);
  CHECK(reconnected.engine->logical_clock() == 0);

  const ApplyResult old =
      reconnected.engine->apply_remote(make_update(42, 7, "bbbb2222", "old"));
  CHECK(old.wrong_generation);
  CHECK(!old.applied);
  CHECK(reconnected.port.writes == 0);
  CHECK(reconnected.sent.empty());
  CHECK(reconnected.engine->logical_clock() == 0);

  EngineOptions options;
  options.self_id = "dddd4444";
  options.generation = 7;
  options.now_ms = [] { return 0; };
  int write_failures = 0;
  SyncEngine without_port(
      nullptr, options, [](const ClipboardUpdate&) { return true; },
      [&](const EngineEvent& event) {
        if (event.kind == EngineEventKind::RemoteWriteFailed) {
          ++write_failures;
        }
      });
  const ApplyResult result =
      without_port.apply_remote(make_update(7, 1, "eeee5555", "hola"));
  CHECK(!result.applied);
  CHECK(result.status.code == StatusCode::Unavailable);
  CHECK(write_failures == 1);
}

}  // namespace

int run_self_tests() {
  std::cout << "oneintwo-clipboard isolated self-tests "
               "(no clipboard, no network, synthetic data)\n";
  run_test("text_validation", test_text_validation);
  run_test("line_endings", test_line_endings);
  run_test("ordering_and_ids", test_ordering_and_ids);
  run_test("fingerprint", test_fingerprint);
  run_test("protocol_roundtrip_and_limits",
           test_protocol_roundtrip_and_limits);
#ifdef OIT_HAVE_PORTAL
  run_test("portal_message_helpers", test_portal_message_helpers);
  run_test("portal_pipe_reads", test_portal_pipe_reads);
#endif
  run_test("engine_local_capture", test_engine_local_capture);
  run_test("engine_remote_and_echo", test_engine_remote_and_echo);
  run_test("engine_rejections", test_engine_rejections);
  run_test("engine_concurrency", test_engine_concurrency);
  run_test("engine_session_isolation", test_engine_session_isolation);

  std::cout << "checks=" << g_checks << " failures=" << g_failures << "\n";
  if (g_failures != 0) {
    std::cerr << "SELF-TEST RESULT: FAIL\n";
    return 1;
  }
  std::cout << "SELF-TEST RESULT: PASS\n";
  return 0;
}

}  // namespace oit
