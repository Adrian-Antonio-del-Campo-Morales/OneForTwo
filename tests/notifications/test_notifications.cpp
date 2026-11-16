#include "support/test_doubles.hpp"
#include "support/test_harness.hpp"

#include <chrono>
#include <cstdint>
#include <string>

using namespace oneintwo;
using namespace oneintwo::tests;

namespace {

std::string repeated(const std::string& unit, std::size_t count) {
    std::string value;
    for (std::size_t index = 0; index < count; ++index) {
        value.append(unit);
    }
    return value;
}

bool contains(const std::vector<std::string>& messages, const std::string& fragment) {
    for (const auto& message : messages) {
        if (message.find(fragment) != std::string::npos) {
            return true;
        }
    }
    return false;
}

ObservedNotification observed(std::string app_id, std::string app_name, std::string title,
                              std::string body = {}) {
    return ObservedNotification{std::move(app_id), std::move(app_name),
                                NotificationContent{std::move(title), std::move(body)}};
}

void start_observation(Fixture& fixture) {
    fixture.module->setAllowedApplications({"app.one"});
    fixture.desktop.emit_state(fixture.desktop.starts.size() - 1, DesktopResult::ready);
}

} // namespace

TEST_CASE(manual_unicode_is_sent_and_presented_with_attribution) {
    Fixture fixture;
    const std::string title = "Reunion — coffee";
    const std::string body = "Line one\nLine two: café, 日本語, emoji 😀";

    fixture.send_manual(title, body);
    REQUIRE(fixture.sender.accepted.size() == 1);
    const RemoteNotification& sent = fixture.sender.accepted.front();
    CHECK(sent.mode == NotificationMode::manual);
    CHECK_EQUAL(sent.ref.generation, fixture.generation);
    CHECK_EQUAL(sent.ref.notice_id, std::uint64_t{1});
    CHECK(sent.app_name.empty());
    CHECK_EQUAL(sent.content.title, title);
    CHECK_EQUAL(sent.content.body, body);

    fixture.receive(fixture.notice(1, NotificationMode::manual, title, body));
    CHECK(fixture.desktop.presentations.empty());
    fixture.tick();
    REQUIRE(fixture.desktop.presentations.size() == 1);
    CHECK_EQUAL(fixture.desktop.presentations.front().peer_name, fixture.peer_name);
    CHECK_EQUAL(fixture.desktop.presentations.front().notice.content.title, title);
    CHECK_EQUAL(fixture.desktop.presentations.front().notice.content.body, body);
    fixture.desktop.presentations.front().complete(DesktopResult::ready);
    CHECK(fixture.errors.empty());
}

TEST_CASE(title_is_required_and_blank_titles_are_rejected) {
    Fixture fixture;
    fixture.send_manual("", "body");
    fixture.send_manual("   \t\n", "body");

    CHECK(fixture.sender.accepted.empty());
    CHECK_EQUAL(fixture.errors.size(), std::size_t{2});
    CHECK(contains(fixture.errors, "the title is compulsory"));
}

TEST_CASE(utf8_byte_limits_are_exact) {
    Fixture fixture;
    const std::string title_at_limit = repeated("\xC3\xA9", 128); // 256 bytes
    fixture.send_manual(title_at_limit);
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{1});

    fixture.clock.advance(std::chrono::seconds(1));
    const std::string title_over_limit = repeated("\xC3\xA9", 129); // 258 bytes
    fixture.send_manual(title_over_limit);
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{1});
    CHECK(contains(fixture.errors, "off-limit content"));

    fixture.clock.advance(std::chrono::seconds(1));
    const std::string body_at_limit = repeated("\xC3\xB1", 1024); // 2048 bytes
    fixture.send_manual("title", body_at_limit);
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{2});

    fixture.clock.advance(std::chrono::seconds(1));
    fixture.send_manual("title", repeated("a", 2049));
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{2});
    CHECK_EQUAL(fixture.errors.size(), std::size_t{2});
}

TEST_CASE(malformed_utf8_is_rejected_in_both_directions) {
    Fixture fixture;
    fixture.send_manual("\xC3", "body");
    fixture.send_manual("title", "\x80");
    fixture.send_manual("\xED\xA0\x80", "body");
    fixture.send_manual("title", "\xC0\xAF");
    CHECK(fixture.sender.accepted.empty());
    CHECK_EQUAL(fixture.errors.size(), std::size_t{4});
    CHECK(contains(fixture.errors, "misformed content"));

    fixture.errors.clear();
    fixture.receive(fixture.notice(1, NotificationMode::manual, "\xE2\x82", "body"));
    fixture.tick();
    CHECK(fixture.desktop.presentations.empty());
    CHECK(contains(fixture.errors, "misformed content"));
}

TEST_CASE(send_rate_is_three_per_monotonic_second) {
    CHECK_EQUAL(notification_rate_per_second, 3u);
    Fixture fixture;
    for (int index = 0; index < 4; ++index) {
        fixture.send_manual("notice " + std::to_string(index));
    }
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{3});
    CHECK_EQUAL(fixture.sender.accepted.back().ref.notice_id, std::uint64_t{3});
    CHECK(contains(fixture.errors, "frequency limit"));

    fixture.clock.advance(std::chrono::seconds(1));
    fixture.send_manual("aviso 4");
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{4});
    CHECK_EQUAL(fixture.sender.accepted.back().ref.notice_id, std::uint64_t{4});
}

TEST_CASE(receive_rate_drop_still_consumes_the_identifier) {
    Fixture fixture;
    for (std::uint64_t identifier = 1; identifier <= 4; ++identifier) {
        fixture.receive(fixture.notice(identifier, NotificationMode::manual,
                                       "notice " + std::to_string(identifier)));
    }
    fixture.tick();
    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{3});
    CHECK(contains(fixture.errors, "frequency limit"));

    fixture.receive(fixture.notice(4, NotificationMode::manual, "repetido"));
    fixture.tick();
    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{3});

    fixture.clock.advance(std::chrono::seconds(1));
    fixture.receive(fixture.notice(5, NotificationMode::manual, "nuevo"));
    fixture.tick();
    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{4});
}

TEST_CASE(outbound_queue_is_bounded_and_retried_in_order) {
    Fixture fixture;
    fixture.sender.default_result = SendResult::busy;
    for (int index = 0; index < 17; ++index) {
        fixture.send_manual("notice " + std::to_string(index));
        fixture.clock.advance(std::chrono::milliseconds(400));
    }
    CHECK_EQUAL(fixture.sender.attempts.size(), std::size_t{17});
    CHECK(contains(fixture.errors, "local full shipping queue"));

    fixture.sender.default_result = SendResult::queued;
    fixture.tick();
    REQUIRE(fixture.sender.accepted.size() == 16);
    for (std::uint64_t index = 0; index < 16; ++index) {
        CHECK_EQUAL(fixture.sender.accepted[index].ref.notice_id, index + 1);
    }
}

TEST_CASE(outbound_queue_expires_after_ten_seconds) {
    Fixture fixture;
    fixture.sender.default_result = SendResult::busy;
    fixture.send_manual("pendiente");
    REQUIRE(fixture.sender.attempts.size() == 1);

    fixture.clock.advance(std::chrono::seconds(10));
    fixture.sender.default_result = SendResult::queued;
    fixture.tick();
    CHECK(fixture.sender.accepted.empty());
    CHECK_EQUAL(fixture.sender.attempts.size(), std::size_t{1});
    CHECK(contains(fixture.errors, "caducada"));
}

TEST_CASE(disconnection_clears_pending_sends) {
    Fixture fixture;
    fixture.sender.script = {SendResult::busy};
    fixture.send_manual("pendiente");
    fixture.sender.script = {SendResult::disconnected};
    fixture.tick();
    CHECK(fixture.sender.accepted.empty());
    CHECK(contains(fixture.errors, "desconectada"));

    fixture.tick();
    CHECK_EQUAL(fixture.sender.attempts.size(), std::size_t{2});
}

TEST_CASE(observed_events_are_filtered_by_identity_and_allow_list) {
    Fixture fixture;
    start_observation(fixture);
    REQUIRE(fixture.desktop.start_count == 1);

    fixture.desktop.emit_observed(0, observed("", "No identity", "title"));
    fixture.desktop.emit_observed(0, observed("OneInTwo.Desktop", "OneInTwo", "title"));
    fixture.desktop.emit_observed(
        0, observed("OneInTwo.Desktop_123!OneInTwo.Desktop", "OneInTwo", "title"));
    fixture.desktop.emit_observed(0, observed("app.two", "Other application", "title"));
    fixture.desktop.emit_observed(0, observed("app.one", "", "title"));
    fixture.desktop.emit_observed(0, observed("app.one", repeated("a", 129), "title"));
    CHECK(fixture.sender.accepted.empty());

    fixture.desktop.emit_observed(0, observed("app.one", "Viewer", "Title", "Body"));
    REQUIRE(fixture.sender.accepted.size() == 1);
    const RemoteNotification& sent = fixture.sender.accepted.front();
    CHECK(sent.mode == NotificationMode::forwarded);
    CHECK_EQUAL(sent.app_name, std::string("Viewer"));
    CHECK_EQUAL(sent.content.title, std::string("Title"));
    CHECK_EQUAL(sent.ref.notice_id, std::uint64_t{1});
}

TEST_CASE(discovery_offers_only_eligible_unique_applications) {
    Fixture fixture;
    std::vector<NotificationApplication> offered{
        {"app.one", "Viewer"},
        {"app.one", "Duplicada"},
        {"app.two", "Dos"},
        {"", "No identity"},
        {"OneInTwo.Desktop", "OneInTwo"},
        {"app.three", ""},
        {"app.four", repeated("a", 129)},
        {"app.five", "Cinco"},
    };

    bool called = false;
    DesktopResult result = DesktopResult::error;
    std::vector<NotificationApplication> received;
    fixture.module->discoverApplications(
        [&](DesktopResult value, std::vector<NotificationApplication> applications) {
            called = true;
            result = value;
            received = std::move(applications);
        });
    REQUIRE(fixture.desktop.discoveries.size() == 1);
    CHECK(!called); // The native consultation is asynchronous.

    fixture.desktop.complete_discovery(0, DesktopResult::ready, std::move(offered));
    CHECK(called);
    CHECK(result == DesktopResult::ready);
    REQUIRE(received.size() == 3);
    CHECK_EQUAL(received[0].app_id, std::string("app.five"));
    CHECK_EQUAL(received[1].app_id, std::string("app.two"));
    CHECK_EQUAL(received[2].app_id, std::string("app.one"));
    CHECK_EQUAL(received[2].app_name, std::string("Viewer"));
    CHECK_EQUAL(fixture.desktop.start_count, 0);
}

TEST_CASE(discovery_is_unavailable_without_observation_support) {
    Fixture fixture;
    fixture.desktop.caps.observe = false;

    bool called = false;
    DesktopResult result = DesktopResult::error;
    std::vector<NotificationApplication> received{{"app.one", "Viewer"}};
    fixture.module->discoverApplications(
        [&](DesktopResult value, std::vector<NotificationApplication> applications) {
            called = true;
            result = value;
            received = std::move(applications);
        });

    CHECK(called);
    CHECK(result == DesktopResult::unavailable);
    CHECK(received.empty());
    CHECK(fixture.desktop.discoveries.empty());
    CHECK_EQUAL(fixture.desktop.start_count, 0);
}

TEST_CASE(discovery_errors_do_not_offer_applications) {
    Fixture fixture;
    bool called = false;
    DesktopResult result = DesktopResult::error;
    std::vector<NotificationApplication> received{{"app.one", "Viewer"}};

    fixture.module->discoverApplications(
        [&](DesktopResult value, std::vector<NotificationApplication> applications) {
            called = true;
            result = value;
            received = std::move(applications);
        });
    REQUIRE(fixture.desktop.discoveries.size() == 1);
    fixture.desktop.complete_discovery(0, DesktopResult::denied,
                                       {{"app.one", "Viewer"}});

    CHECK(called);
    CHECK(result == DesktopResult::denied);
    CHECK(received.empty());
}

TEST_CASE(discovery_callback_is_discarded_after_module_destruction) {
    Fixture fixture;
    bool called = false;
    fixture.module->discoverApplications(
        [&](DesktopResult, std::vector<NotificationApplication>) { called = true; });
    REQUIRE(fixture.desktop.discoveries.size() == 1);
    const auto complete = fixture.desktop.discoveries.front().complete;

    fixture.module.reset();
    complete(DesktopResult::ready, {{"app.one", "Viewer"}});
    CHECK(!called);
}

TEST_CASE(observed_duplicates_are_suppressed_within_the_window) {
    Fixture fixture;
    start_observation(fixture);

    fixture.desktop.emit_observed(0, observed("app.one", "Viewer", "Title", "Body"));
    fixture.desktop.emit_observed(0, observed("app.one", "Viewer", "Title", "Body"));
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{1});

    fixture.desktop.emit_observed(0, observed("app.one", "Viewer", "Title", "Otro cuerpo"));
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{2});

    fixture.clock.advance(std::chrono::seconds(11));
    fixture.desktop.emit_observed(0, observed("app.one", "Viewer", "Title", "Body"));
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{3});
}

TEST_CASE(manual_and_forwarded_share_the_send_counter) {
    Fixture fixture;
    start_observation(fixture);
    fixture.send_manual("manual uno");
    fixture.desktop.emit_observed(0, observed("app.one", "Viewer", "automatic"));
    fixture.send_manual("manual dos");

    REQUIRE(fixture.sender.accepted.size() == 3);
    CHECK(fixture.sender.accepted[0].mode == NotificationMode::manual);
    CHECK_EQUAL(fixture.sender.accepted[0].ref.notice_id, std::uint64_t{1});
    CHECK(fixture.sender.accepted[1].mode == NotificationMode::forwarded);
    CHECK_EQUAL(fixture.sender.accepted[1].ref.notice_id, std::uint64_t{2});
    CHECK(fixture.sender.accepted[2].mode == NotificationMode::manual);
    CHECK_EQUAL(fixture.sender.accepted[2].ref.notice_id, std::uint64_t{3});
}

TEST_CASE(received_notices_are_validated_before_deduplication) {
    Fixture fixture;
    fixture.receive(fixture.notice(1, NotificationMode::manual, "title", {}, "App"));
    fixture.receive(fixture.notice(1, NotificationMode::manual, "title"));
    fixture.receive(fixture.notice(2, NotificationMode::forwarded, "title", {}, ""));
    fixture.receive(fixture.notice(2, NotificationMode::forwarded, "title", {}, "Viewer"));
    fixture.receive(fixture.notice(3, NotificationMode::manual, repeated("a", 257)));
    fixture.receive(fixture.notice(3, NotificationMode::manual, "title"));
    fixture.receive(fixture.notice(4, NotificationMode::manual, "title", repeated("b", 2049)));
    fixture.receive(fixture.notice(5, NotificationMode::forwarded, "title", {},
                                   repeated("a", 129)));
    fixture.receive(fixture.notice(0, NotificationMode::manual, "title"));
    fixture.tick();

    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{3});
    CHECK(contains(fixture.errors, "manual warning with application"));
    CHECK(contains(fixture.errors, "application of invalid origin"));
    CHECK(contains(fixture.errors, "invalid identifier"));
}

TEST_CASE(repetitions_and_older_received_identifiers_are_discarded) {
    Fixture fixture;
    fixture.receive(fixture.notice(5, NotificationMode::manual, "cinco"));
    fixture.tick();
    REQUIRE(fixture.desktop.presentations.size() == 1);

    fixture.receive(fixture.notice(5, NotificationMode::manual, "cinco repetido"));
    fixture.tick();
    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{1});

    fixture.receive(fixture.notice(4, NotificationMode::manual, "cuatro antiguo"));
    fixture.tick();
    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{1});

    fixture.receive(fixture.notice(6, NotificationMode::manual, "seis"));
    fixture.tick();
    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{2});
}

TEST_CASE(receive_queue_is_bounded) {
    Fixture fixture;
    for (std::uint64_t identifier = 1; identifier <= 17; ++identifier) {
        fixture.receive(fixture.notice(identifier, NotificationMode::manual,
                                       "notice " + std::to_string(identifier)));
        fixture.clock.advance(std::chrono::milliseconds(400));
    }
    CHECK(contains(fixture.errors, "local full presentation queue"));

    fixture.tick();
    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{16});
}

TEST_CASE(receive_queue_expires_after_ten_seconds) {
    Fixture fixture;
    fixture.receive(fixture.notice(1, NotificationMode::manual, "caduca"));
    fixture.clock.advance(std::chrono::seconds(10));
    fixture.tick();
    CHECK(fixture.desktop.presentations.empty());
    CHECK(contains(fixture.errors, "Expired glue"));
}

TEST_CASE(revocation_withdraws_notices_before_presentation) {
    Fixture fixture;
    fixture.receive(fixture.notice(1, NotificationMode::manual, "pendiente"));
    fixture.receive_features = FeatureSet{};
    fixture.authorize();
    CHECK(fixture.desktop.withdraw_count >= 1);
    fixture.tick();
    CHECK(fixture.desktop.presentations.empty());

    // Reauthorize does not reproduce the notice already consumed.
    fixture.receive_features.manual_notifications = true;
    fixture.authorize();
    fixture.receive(fixture.notice(1, NotificationMode::manual, "pendiente"));
    fixture.tick();
    CHECK(fixture.desktop.presentations.empty());

    fixture.receive(fixture.notice(2, NotificationMode::manual, "nuevo"));
    fixture.tick();
    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{1});
}

TEST_CASE(presentation_completion_after_revocation_is_ignored) {
    Fixture fixture;
    fixture.receive(fixture.notice(1, NotificationMode::manual, "visible"));
    fixture.tick();
    REQUIRE(fixture.desktop.presentations.size() == 1);
    const auto complete = fixture.desktop.presentations.front().complete;

    fixture.receive_features = FeatureSet{};
    fixture.authorize();

    complete(DesktopResult::ready);
    complete(DesktopResult::error);
    CHECK(fixture.errors.empty());
    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{1});
}

TEST_CASE(send_permission_revocation_drops_pending_sends) {
    Fixture fixture;
    fixture.sender.default_result = SendResult::busy;
    fixture.send_manual("pendiente");
    REQUIRE(fixture.sender.attempts.size() == 1);

    fixture.send_features = FeatureSet{};
    fixture.receive_features = FeatureSet{};
    fixture.authorize();
    fixture.sender.default_result = SendResult::queued;
    fixture.tick();
    CHECK(fixture.sender.accepted.empty());
    CHECK_EQUAL(fixture.sender.attempts.size(), std::size_t{1});
}

TEST_CASE(new_generation_resets_counters_and_restarts_observation) {
    Fixture fixture;
    start_observation(fixture);
    REQUIRE(fixture.desktop.start_count == 1);

    fixture.send_manual("generation seven");
    REQUIRE(fixture.sender.accepted.size() == 1);
    CHECK_EQUAL(fixture.sender.accepted.front().ref.notice_id, std::uint64_t{1});
    fixture.receive(fixture.notice(1, NotificationMode::manual, "pendiente"));

    fixture.generation = 8;
    fixture.authorize();
    CHECK(fixture.desktop.stop_count >= 1);
    CHECK_EQUAL(fixture.desktop.start_count, 2);
    CHECK(fixture.desktop.withdraw_count >= 1);

    fixture.tick();
    CHECK(fixture.desktop.presentations.empty());

    // The ancient generation does not present or send again.
    fixture.module->receiveNotice(RemoteNotification{
        NotificationRef{7, 9}, NotificationMode::manual,
        NotificationContent{"antiguo", {}}, {}});
    fixture.tick();
    CHECK(fixture.desktop.presentations.empty());

    fixture.send_manual("generation eight");
    REQUIRE(fixture.sender.accepted.size() == 2);
    CHECK_EQUAL(fixture.sender.accepted.back().ref.generation, std::uint64_t{8});
    CHECK_EQUAL(fixture.sender.accepted.back().ref.notice_id, std::uint64_t{1});
}

TEST_CASE(lock_withdraws_everything_and_keeps_observation_stopped) {
    Fixture fixture;
    start_observation(fixture);
    fixture.send_manual("before blocking");
    fixture.receive(fixture.notice(1, NotificationMode::manual, "pendiente"));

    fixture.send_features = FeatureSet{};
    fixture.receive_features = FeatureSet{};
    fixture.authorize();
    CHECK(fixture.desktop.stop_count >= 1);
    CHECK(fixture.desktop.withdraw_count >= 1);

    fixture.tick();
    CHECK(fixture.desktop.presentations.empty());

    fixture.send_manual("during blocking");
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{1});
    CHECK(contains(fixture.errors, "shipping permit not granted"));
}

TEST_CASE(solo_envios_keeps_manual_and_stops_forwarding) {
    Fixture fixture;
    fixture.send_features.manual_notifications = true;
    fixture.send_features.forwarded_notifications = false;
    fixture.receive_features = FeatureSet{};
    fixture.receive_features.manual_notifications = true;
    fixture.authorize();

    fixture.module->setAllowedApplications({"app.one"});
    CHECK_EQUAL(fixture.desktop.start_count, 0);

    fixture.module->forwardObserved(fixture.generation,
                                    observed("app.one", "Viewer", "automatic"));
    CHECK(fixture.sender.accepted.empty());

    fixture.send_manual("manual permitido");
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{1});

    fixture.receive(fixture.notice(1, NotificationMode::manual, "manual recibido"));
    fixture.receive(fixture.notice(2, NotificationMode::forwarded, "automatic", {}, "Viewer"));
    fixture.tick();
    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{1});
}

TEST_CASE(allowed_application_changes_update_observation) {
    Fixture fixture;
    fixture.module->setAllowedApplications({"app.one"});
    REQUIRE(fixture.desktop.start_count == 1);

    fixture.module->setAllowedApplications({});
    CHECK_EQUAL(fixture.desktop.stop_count, 1);

    fixture.module->setAllowedApplications({"app.one", "app.two"});
    CHECK_EQUAL(fixture.desktop.start_count, 2);
    REQUIRE(fixture.desktop.starts.size() == 2);

    // An event from the previous subscription is not delivered.
    fixture.desktop.emit_observed(0, observed("app.two", "Dos", "viejo"));
    CHECK(fixture.sender.accepted.empty());

    fixture.desktop.emit_observed(1, observed("app.two", "Dos", "nuevo"));
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{1});
}

TEST_CASE(denied_observation_stops_and_reports) {
    Fixture fixture;
    start_observation(fixture);
    REQUIRE(fixture.desktop.start_count == 1);

    fixture.desktop.emit_state(0, DesktopResult::denied);
    CHECK(fixture.desktop.stop_count >= 1);
    CHECK(contains(fixture.errors, "denegada"));

    fixture.desktop.emit_observed(0, observed("app.one", "Viewer", "title"));
    CHECK(fixture.sender.accepted.empty());
}

TEST_CASE(late_observed_events_from_stopped_subscriptions_are_discarded) {
    Fixture fixture;
    fixture.module->setAllowedApplications({"app.one"});
    REQUIRE(fixture.desktop.start_count == 1);
    const auto stale = fixture.desktop.starts.front().observed;

    fixture.module->setAllowedApplications({});
    stale(observed("app.one", "Viewer", "during the stop"));
    CHECK(fixture.sender.accepted.empty());

    fixture.module->setAllowedApplications({"app.one"});
    REQUIRE(fixture.desktop.start_count == 2);
    stale(observed("app.one", "Viewer", "tras reanudar"));
    CHECK(fixture.sender.accepted.empty());

    fixture.desktop.emit_observed(1, observed("app.one", "Viewer", "current subscription"));
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{1});
}

TEST_CASE(presentation_failure_reports_without_content) {
    Fixture fixture;
    fixture.receive(fixture.notice(1, NotificationMode::manual, "secret title",
                                   "cuerpo secreto"));
    fixture.tick();
    REQUIRE(fixture.desktop.presentations.size() == 1);
    fixture.desktop.presentations.front().complete(DesktopResult::error);

    REQUIRE(fixture.errors.size() == 1);
    CHECK(fixture.errors.front().find("desktop error") != std::string::npos);
    CHECK(fixture.errors.front().find("secreto") == std::string::npos);
    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{1});
}

TEST_CASE(presentation_requires_a_verified_peer_name) {
    Fixture fixture;
    fixture.peer_name.clear();
    fixture.receive(fixture.notice(1, NotificationMode::manual, "title"));
    fixture.tick();
    CHECK(fixture.desktop.presentations.empty());
    CHECK(contains(fixture.errors, "identity of equipment not available"));
}

TEST_CASE(observation_requires_backend_capability) {
    Fixture fixture;
    fixture.desktop.caps.observe = false;
    fixture.module->setAllowedApplications({"app.one"});
    CHECK_EQUAL(fixture.desktop.start_count, 0);
}

TEST_CASE(received_notice_without_permission_does_not_consume_identifier) {
    Fixture fixture;
    fixture.receive_features = FeatureSet{};
    fixture.authorize();
    fixture.receive(fixture.notice(1, NotificationMode::manual, "no autorizado"));
    fixture.tick();
    CHECK(fixture.desktop.presentations.empty());

    fixture.receive_features.manual_notifications = true;
    fixture.authorize();
    fixture.receive(fixture.notice(1, NotificationMode::manual, "autorizado"));
    fixture.tick();
    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{1});
}

TEST_CASE(observed_notice_before_subscription_is_discarded) {
    Fixture fixture;
    // There is still no list of applications or observation initiated.
    fixture.module->forwardObserved(fixture.generation,
                                    observed("app.one", "Viewer", "previo"));
    CHECK(fixture.sender.accepted.empty());
    CHECK_EQUAL(fixture.desktop.start_count, 0);
}

TEST_CASE(stale_generations_are_ignored_on_send) {
    Fixture fixture;
    fixture.module->sendManual(fixture.generation + 1,
                               NotificationContent{"antiguo", "body"});
    CHECK(fixture.sender.accepted.empty());
    CHECK(fixture.errors.empty());
}
