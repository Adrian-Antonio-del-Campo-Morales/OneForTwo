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
    const std::string title = "Reunión — café ☕";
    const std::string body = "Línea uno\nLínea dos: ñandú, 日本語, emoji 😀";

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
    fixture.send_manual("", "cuerpo");
    fixture.send_manual("   \t\n", "cuerpo");

    CHECK(fixture.sender.accepted.empty());
    CHECK_EQUAL(fixture.errors.size(), std::size_t{2});
    CHECK(contains(fixture.errors, "el título es obligatorio"));
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
    CHECK(contains(fixture.errors, "contenido fuera de límite"));

    fixture.clock.advance(std::chrono::seconds(1));
    const std::string body_at_limit = repeated("\xC3\xB1", 1024); // 2048 bytes
    fixture.send_manual("título", body_at_limit);
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{2});

    fixture.clock.advance(std::chrono::seconds(1));
    fixture.send_manual("título", repeated("a", 2049));
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{2});
    CHECK_EQUAL(fixture.errors.size(), std::size_t{2});
}

TEST_CASE(malformed_utf8_is_rejected_in_both_directions) {
    Fixture fixture;
    fixture.send_manual("\xC3", "cuerpo");
    fixture.send_manual("título", "\x80");
    fixture.send_manual("\xED\xA0\x80", "cuerpo");
    fixture.send_manual("título", "\xC0\xAF");
    CHECK(fixture.sender.accepted.empty());
    CHECK_EQUAL(fixture.errors.size(), std::size_t{4});
    CHECK(contains(fixture.errors, "contenido mal formado"));

    fixture.errors.clear();
    fixture.receive(fixture.notice(1, NotificationMode::manual, "\xE2\x82", "cuerpo"));
    fixture.tick();
    CHECK(fixture.desktop.presentations.empty());
    CHECK(contains(fixture.errors, "contenido mal formado"));
}

TEST_CASE(send_rate_is_three_per_monotonic_second) {
    Fixture fixture;
    for (int index = 0; index < 4; ++index) {
        fixture.send_manual("aviso " + std::to_string(index));
    }
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{3});
    CHECK_EQUAL(fixture.sender.accepted.back().ref.notice_id, std::uint64_t{3});
    CHECK(contains(fixture.errors, "límite de frecuencia"));

    fixture.clock.advance(std::chrono::seconds(1));
    fixture.send_manual("aviso 4");
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{4});
    CHECK_EQUAL(fixture.sender.accepted.back().ref.notice_id, std::uint64_t{4});
}

TEST_CASE(receive_rate_drop_still_consumes_the_identifier) {
    Fixture fixture;
    for (std::uint64_t identifier = 1; identifier <= 4; ++identifier) {
        fixture.receive(fixture.notice(identifier, NotificationMode::manual,
                                       "aviso " + std::to_string(identifier)));
    }
    fixture.tick();
    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{3});
    CHECK(contains(fixture.errors, "límite de frecuencia"));

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
        fixture.send_manual("aviso " + std::to_string(index));
        fixture.clock.advance(std::chrono::milliseconds(400));
    }
    CHECK_EQUAL(fixture.sender.attempts.size(), std::size_t{17});
    CHECK(contains(fixture.errors, "cola local de envío llena"));

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

    fixture.desktop.emit_observed(0, observed("", "Sin identidad", "título"));
    fixture.desktop.emit_observed(0, observed("OneInTwo.Desktop", "OneInTwo", "título"));
    fixture.desktop.emit_observed(
        0, observed("OneInTwo.Desktop_123!OneInTwo.Desktop", "OneInTwo", "título"));
    fixture.desktop.emit_observed(0, observed("app.two", "Otra aplicación", "título"));
    fixture.desktop.emit_observed(0, observed("app.one", "", "título"));
    fixture.desktop.emit_observed(0, observed("app.one", repeated("a", 129), "título"));
    CHECK(fixture.sender.accepted.empty());

    fixture.desktop.emit_observed(0, observed("app.one", "Visor", "Título", "Cuerpo"));
    REQUIRE(fixture.sender.accepted.size() == 1);
    const RemoteNotification& sent = fixture.sender.accepted.front();
    CHECK(sent.mode == NotificationMode::forwarded);
    CHECK_EQUAL(sent.app_name, std::string("Visor"));
    CHECK_EQUAL(sent.content.title, std::string("Título"));
    CHECK_EQUAL(sent.ref.notice_id, std::uint64_t{1});
}

TEST_CASE(observed_duplicates_are_suppressed_within_the_window) {
    Fixture fixture;
    start_observation(fixture);

    fixture.desktop.emit_observed(0, observed("app.one", "Visor", "Título", "Cuerpo"));
    fixture.desktop.emit_observed(0, observed("app.one", "Visor", "Título", "Cuerpo"));
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{1});

    fixture.desktop.emit_observed(0, observed("app.one", "Visor", "Título", "Otro cuerpo"));
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{2});

    fixture.clock.advance(std::chrono::seconds(11));
    fixture.desktop.emit_observed(0, observed("app.one", "Visor", "Título", "Cuerpo"));
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{3});
}

TEST_CASE(manual_and_forwarded_share_the_send_counter) {
    Fixture fixture;
    start_observation(fixture);
    fixture.send_manual("manual uno");
    fixture.desktop.emit_observed(0, observed("app.one", "Visor", "automático"));
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
    fixture.receive(fixture.notice(1, NotificationMode::manual, "título", {}, "App"));
    fixture.receive(fixture.notice(1, NotificationMode::manual, "título"));
    fixture.receive(fixture.notice(2, NotificationMode::forwarded, "título", {}, ""));
    fixture.receive(fixture.notice(2, NotificationMode::forwarded, "título", {}, "Visor"));
    fixture.receive(fixture.notice(3, NotificationMode::manual, repeated("a", 257)));
    fixture.receive(fixture.notice(3, NotificationMode::manual, "título"));
    fixture.receive(fixture.notice(4, NotificationMode::manual, "título", repeated("b", 2049)));
    fixture.receive(fixture.notice(5, NotificationMode::forwarded, "título", {},
                                   repeated("a", 129)));
    fixture.receive(fixture.notice(0, NotificationMode::manual, "título"));
    fixture.tick();

    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{3});
    CHECK(contains(fixture.errors, "aviso manual con aplicación"));
    CHECK(contains(fixture.errors, "aplicación de origen inválida"));
    CHECK(contains(fixture.errors, "identificador inválido"));
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
                                       "aviso " + std::to_string(identifier)));
        fixture.clock.advance(std::chrono::milliseconds(400));
    }
    CHECK(contains(fixture.errors, "cola local de presentación llena"));

    fixture.tick();
    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{16});
}

TEST_CASE(receive_queue_expires_after_ten_seconds) {
    Fixture fixture;
    fixture.receive(fixture.notice(1, NotificationMode::manual, "caduca"));
    fixture.clock.advance(std::chrono::seconds(10));
    fixture.tick();
    CHECK(fixture.desktop.presentations.empty());
    CHECK(contains(fixture.errors, "cola caducada"));
}

TEST_CASE(revocation_withdraws_notices_before_presentation) {
    Fixture fixture;
    fixture.receive(fixture.notice(1, NotificationMode::manual, "pendiente"));
    fixture.receive_features = FeatureSet{};
    fixture.authorize();
    CHECK(fixture.desktop.withdraw_count >= 1);
    fixture.tick();
    CHECK(fixture.desktop.presentations.empty());

    // Reautorizar no reproduce el aviso ya consumido.
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

    fixture.send_manual("generación siete");
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

    // La generación antigua no vuelve a presentar ni enviar.
    fixture.module->receiveNotice(RemoteNotification{
        NotificationRef{7, 9}, NotificationMode::manual,
        NotificationContent{"antiguo", {}}, {}});
    fixture.tick();
    CHECK(fixture.desktop.presentations.empty());

    fixture.send_manual("generación ocho");
    REQUIRE(fixture.sender.accepted.size() == 2);
    CHECK_EQUAL(fixture.sender.accepted.back().ref.generation, std::uint64_t{8});
    CHECK_EQUAL(fixture.sender.accepted.back().ref.notice_id, std::uint64_t{1});
}

TEST_CASE(lock_withdraws_everything_and_keeps_observation_stopped) {
    Fixture fixture;
    start_observation(fixture);
    fixture.send_manual("antes del bloqueo");
    fixture.receive(fixture.notice(1, NotificationMode::manual, "pendiente"));

    fixture.send_features = FeatureSet{};
    fixture.receive_features = FeatureSet{};
    fixture.authorize();
    CHECK(fixture.desktop.stop_count >= 1);
    CHECK(fixture.desktop.withdraw_count >= 1);

    fixture.tick();
    CHECK(fixture.desktop.presentations.empty());

    fixture.send_manual("durante el bloqueo");
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{1});
    CHECK(contains(fixture.errors, "permiso de envío no concedido"));
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
                                    observed("app.one", "Visor", "automático"));
    CHECK(fixture.sender.accepted.empty());

    fixture.send_manual("manual permitido");
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{1});

    fixture.receive(fixture.notice(1, NotificationMode::manual, "manual recibido"));
    fixture.receive(fixture.notice(2, NotificationMode::forwarded, "automático", {}, "Visor"));
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

    // Un evento de la suscripción anterior no se entrega.
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

    fixture.desktop.emit_observed(0, observed("app.one", "Visor", "título"));
    CHECK(fixture.sender.accepted.empty());
}

TEST_CASE(late_observed_events_from_stopped_subscriptions_are_discarded) {
    Fixture fixture;
    fixture.module->setAllowedApplications({"app.one"});
    REQUIRE(fixture.desktop.start_count == 1);
    const auto stale = fixture.desktop.starts.front().observed;

    fixture.module->setAllowedApplications({});
    stale(observed("app.one", "Visor", "durante la parada"));
    CHECK(fixture.sender.accepted.empty());

    fixture.module->setAllowedApplications({"app.one"});
    REQUIRE(fixture.desktop.start_count == 2);
    stale(observed("app.one", "Visor", "tras reanudar"));
    CHECK(fixture.sender.accepted.empty());

    fixture.desktop.emit_observed(1, observed("app.one", "Visor", "suscripción vigente"));
    CHECK_EQUAL(fixture.sender.accepted.size(), std::size_t{1});
}

TEST_CASE(presentation_failure_reports_without_content) {
    Fixture fixture;
    fixture.receive(fixture.notice(1, NotificationMode::manual, "título secreto",
                                   "cuerpo secreto"));
    fixture.tick();
    REQUIRE(fixture.desktop.presentations.size() == 1);
    fixture.desktop.presentations.front().complete(DesktopResult::error);

    REQUIRE(fixture.errors.size() == 1);
    CHECK(fixture.errors.front().find("error del escritorio") != std::string::npos);
    CHECK(fixture.errors.front().find("secreto") == std::string::npos);
    CHECK_EQUAL(fixture.desktop.presentations.size(), std::size_t{1});
}

TEST_CASE(presentation_requires_a_verified_peer_name) {
    Fixture fixture;
    fixture.peer_name.clear();
    fixture.receive(fixture.notice(1, NotificationMode::manual, "título"));
    fixture.tick();
    CHECK(fixture.desktop.presentations.empty());
    CHECK(contains(fixture.errors, "identidad del equipo no disponible"));
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
    // Todavía no hay lista de aplicaciones ni observación iniciada.
    fixture.module->forwardObserved(fixture.generation,
                                    observed("app.one", "Visor", "previo"));
    CHECK(fixture.sender.accepted.empty());
    CHECK_EQUAL(fixture.desktop.start_count, 0);
}

TEST_CASE(stale_generations_are_ignored_on_send) {
    Fixture fixture;
    fixture.module->sendManual(fixture.generation + 1,
                               NotificationContent{"antiguo", "cuerpo"});
    CHECK(fixture.sender.accepted.empty());
    CHECK(fixture.errors.empty());
}
