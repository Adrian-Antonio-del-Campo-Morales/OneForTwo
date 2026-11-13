#include "platform/windows/notification_support/observation_baseline.hpp"
#include "platform/windows/notification_support/toast_text.hpp"
#include "support/test_harness.hpp"

#include <string>

using namespace oneintwo::tests;
using namespace oneintwo::windows_notifications;

TEST_CASE(toast_xml_escapes_markup_and_controls) {
    CHECK_EQUAL(escape_xml_text("a & b < c > d \"e\" 'f'"),
                std::string("a &amp; b &lt; c &gt; d &quot;e&quot; &apos;f&apos;"));
    CHECK_EQUAL(escape_xml_text(std::string("line\x01")), std::string("line "));
    CHECK_EQUAL(escape_xml_text("\xE6\x97\xA5 & \xF0\x9F\x98\x80"),
                std::string("\xE6\x97\xA5 &amp; \xF0\x9F\x98\x80"));
}

TEST_CASE(toast_xml_presents_plain_text_with_attribution) {
    ToastText manual;
    manual.title = "Reunión <hoy>";
    manual.body = "Cuerpo & texto";
    manual.peer_name = "PORTATIL-A";
    const std::string manual_xml = build_toast_xml(manual);
    CHECK(manual_xml.find("<toast") == 0);
    CHECK(manual_xml.find("ToastGeneric") != std::string::npos);
    CHECK(manual_xml.find("<text>Reunión &lt;hoy&gt;</text>") != std::string::npos);
    CHECK(manual_xml.find("<text>Cuerpo &amp; texto</text>") != std::string::npos);
    CHECK(manual_xml.find("<text>De PORTATIL-A</text>") != std::string::npos);
    CHECK(manual_xml.find("<actions") == std::string::npos);
    CHECK(manual_xml.find("launch=") == std::string::npos);

    ToastText forwarded = manual;
    forwarded.app_name = "Visor de Documentos";
    const std::string forwarded_xml = build_toast_xml(forwarded);
    CHECK(forwarded_xml.find("<text>Reunión &lt;hoy&gt;</text>") != std::string::npos);
    CHECK(forwarded_xml.find("<text>Visor de Documentos</text>") != std::string::npos);
    CHECK(forwarded_xml.find("<text>Cuerpo &amp; texto</text>") != std::string::npos);
    CHECK(forwarded_xml.find("<text>De PORTATIL-A</text>") != std::string::npos);
}

TEST_CASE(observation_baseline_only_accepts_new_identifiers) {
    ObservationBaseline baseline;
    baseline.seed({10, 11, 12});
    CHECK_EQUAL(baseline.baseline_size(), std::size_t{3});

    CHECK(!baseline.accept(10));
    CHECK(!baseline.accept(11));
    CHECK(baseline.accept(13));
    CHECK(!baseline.accept(13));
    CHECK(baseline.accept(14));
    CHECK_EQUAL(baseline.seen_size(), std::size_t{4});
}

TEST_CASE(observation_baseline_restarts_after_reconnection) {
    ObservationBaseline before;
    before.seed({20});
    CHECK(before.accept(21));

    // Nueva línea base: 21 ya existía al volver a observar.
    ObservationBaseline after;
    after.seed({20, 21});
    CHECK(!after.accept(21));
    CHECK(after.accept(22));
}
