/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Network System", "index.html", [
    [ "System Overview", "index.html#overview", null ],
    [ "Key Features", "index.html#features", null ],
    [ "Architecture Diagram", "index.html#architecture", null ],
    [ "Quick Start", "index.html#quickstart", null ],
    [ "Installation", "index.html#installation", [
      [ "CMake FetchContent (Recommended)", "index.html#install_fetchcontent", null ],
      [ "vcpkg", "index.html#install_vcpkg", null ],
      [ "Manual Clone", "index.html#install_manual", null ]
    ] ],
    [ "Module Overview", "index.html#modules", null ],
    [ "Examples", "index.html#examples", null ],
    [ "Learning Resources", "index.html#learning", null ],
    [ "Related Systems", "index.html#related", null ],
    [ "Core Module", "md_include_2kcenon_2network_2core_2README.html", [
      [ "Contents", "md_include_2kcenon_2network_2core_2README.html#autotoc_md1", [
        [ "Stable APIs", "md_include_2kcenon_2network_2core_2README.html#autotoc_md2", null ],
        [ "Compatibility Headers (Deprecated)", "md_include_2kcenon_2network_2core_2README.html#autotoc_md3", null ]
      ] ],
      [ "Stability", "md_include_2kcenon_2network_2core_2README.html#autotoc_md4", null ],
      [ "Usage", "md_include_2kcenon_2network_2core_2README.html#autotoc_md5", null ],
      [ "Namespace", "md_include_2kcenon_2network_2core_2README.html#autotoc_md6", null ],
      [ "Module Organization", "md_include_2kcenon_2network_2core_2README.html#autotoc_md7", null ]
    ] ],
    [ "Experimental Module (Migrated)", "md_include_2kcenon_2network_2experimental_2README.html", [
      [ "Status: Moved to Internal", "md_include_2kcenon_2network_2experimental_2README.html#autotoc_md106", null ],
      [ "Migration Guide", "md_include_2kcenon_2network_2experimental_2README.html#autotoc_md107", null ],
      [ "Recommended Approach", "md_include_2kcenon_2network_2experimental_2README.html#autotoc_md108", null ],
      [ "Why This Change?", "md_include_2kcenon_2network_2experimental_2README.html#autotoc_md109", null ],
      [ "See Also", "md_include_2kcenon_2network_2experimental_2README.html#autotoc_md110", null ]
    ] ],
    [ "HTTP Module", "md_include_2kcenon_2network_2http_2README.html", [
      [ "Contents", "md_include_2kcenon_2network_2http_2README.html#autotoc_md155", null ],
      [ "Stability", "md_include_2kcenon_2network_2http_2README.html#autotoc_md156", null ],
      [ "Usage", "md_include_2kcenon_2network_2http_2README.html#autotoc_md157", null ],
      [ "Migration from core/", "md_include_2kcenon_2network_2http_2README.html#autotoc_md158", null ],
      [ "Namespace", "md_include_2kcenon_2network_2http_2README.html#autotoc_md159", null ]
    ] ],
    [ "README", "md_README.html", [
      [ "Network System", "md_README.html#autotoc_md553", [
        [ "Table of Contents", "md_README.html#autotoc_md554", null ],
        [ "Overview", "md_README.html#autotoc_md556", null ],
        [ "Installation via vcpkg", "md_README.html#autotoc_md558", [
          [ "Quick Start", "md_README.html#autotoc_md559", null ],
          [ "Feature Matrix", "md_README.html#autotoc_md560", null ],
          [ "CMake Integration", "md_README.html#autotoc_md561", null ],
          [ "Minimal Example", "md_README.html#autotoc_md562", null ]
        ] ],
        [ "Requirements", "md_README.html#autotoc_md564", [
          [ "Dependency Flow", "md_README.html#autotoc_md565", null ],
          [ "Building with Dependencies", "md_README.html#autotoc_md566", null ]
        ] ],
        [ "Quick Start", "md_README.html#autotoc_md568", [
          [ "Prerequisites", "md_README.html#autotoc_md569", null ],
          [ "Build", "md_README.html#autotoc_md570", null ],
          [ "C++20 Module Build (Experimental)", "md_README.html#autotoc_md571", null ],
          [ "Your First Server (60 seconds)", "md_README.html#autotoc_md572", null ],
          [ "Your First Client", "md_README.html#autotoc_md573", null ],
          [ "Simplified Facade API (NEW in v2.0)", "md_README.html#autotoc_md574", null ]
        ] ],
        [ "Modular Architecture (NEW)", "md_README.html#autotoc_md576", [
          [ "Library Overview", "md_README.html#autotoc_md577", null ],
          [ "Dependency Graph", "md_README.html#autotoc_md578", null ],
          [ "Selective Linking", "md_README.html#autotoc_md579", null ],
          [ "Umbrella Header", "md_README.html#autotoc_md580", null ]
        ] ],
        [ "Core Features", "md_README.html#autotoc_md582", [
          [ "Protocols", "md_README.html#autotoc_md583", null ],
          [ "Distributed Tracing", "md_README.html#autotoc_md584", null ],
          [ "Asynchronous Model", "md_README.html#autotoc_md585", null ],
          [ "Failure Handling", "md_README.html#autotoc_md586", null ],
          [ "Error Handling", "md_README.html#autotoc_md587", null ]
        ] ],
        [ "Performance Highlights", "md_README.html#autotoc_md589", [
          [ "Synthetic Benchmarks (Intel i7-12700K, Ubuntu 22.04, GCC 11, -O3)", "md_README.html#autotoc_md590", null ],
          [ "Real I/O Benchmarks (Loopback TCP)", "md_README.html#autotoc_md591", null ],
          [ "Reproducing Benchmarks", "md_README.html#autotoc_md592", null ]
        ] ],
        [ "Architecture Overview", "md_README.html#autotoc_md594", null ],
        [ "Ecosystem Integration", "md_README.html#autotoc_md596", [
          [ "Ecosystem Dependency Map", "md_README.html#autotoc_md597", null ],
          [ "Related Projects", "md_README.html#autotoc_md598", null ],
          [ "Integration Example", "md_README.html#autotoc_md599", null ],
          [ "Thread Pool Adapters", "md_README.html#autotoc_md600", null ]
        ] ],
        [ "Documentation", "md_README.html#autotoc_md602", [
          [ "Getting Started", "md_README.html#autotoc_md603", null ],
          [ "Advanced Topics", "md_README.html#autotoc_md604", null ],
          [ "Development", "md_README.html#autotoc_md605", null ]
        ] ],
        [ "Platform Support", "md_README.html#autotoc_md607", null ],
        [ "Production Quality", "md_README.html#autotoc_md609", [
          [ "CI/CD Infrastructure", "md_README.html#autotoc_md610", null ],
          [ "Security", "md_README.html#autotoc_md611", null ],
          [ "Thread Safety & Memory Safety", "md_README.html#autotoc_md612", null ]
        ] ],
        [ "Dependencies", "md_README.html#autotoc_md614", [
          [ "Required", "md_README.html#autotoc_md615", null ]
        ] ],
        [ "", "md_README.html#autotoc_md616", null ],
        [ "Build Options", "md_README.html#autotoc_md617", [
          [ "Using CMake Presets (Recommended)", "md_README.html#autotoc_md618", null ],
          [ "Manual CMake Configuration", "md_README.html#autotoc_md619", null ],
          [ "Cleaning Build Directories", "md_README.html#autotoc_md620", null ],
          [ "Available CMake Options", "md_README.html#autotoc_md621", null ]
        ] ],
        [ "Examples", "md_README.html#autotoc_md623", null ],
        [ "Roadmap", "md_README.html#autotoc_md625", [
          [ "Recently Completed", "md_README.html#autotoc_md626", null ],
          [ "Current Focus", "md_README.html#autotoc_md627", null ],
          [ "Planned Features", "md_README.html#autotoc_md628", null ]
        ] ],
        [ "Contributing", "md_README.html#autotoc_md630", null ],
        [ "License", "md_README.html#autotoc_md632", null ],
        [ "Contact & Support", "md_README.html#autotoc_md634", null ],
        [ "Acknowledgments", "md_README.html#autotoc_md636", [
          [ "Core Dependencies", "md_README.html#autotoc_md637", null ],
          [ "Ecosystem Integration", "md_README.html#autotoc_md638", null ]
        ] ]
      ] ]
    ] ],
    [ "Tutorial: TCP Client and Server", "tutorial_tcp.html", [
      [ "Introduction", "tutorial_tcp.html#tcp_intro", null ],
      [ "Core Concepts", "tutorial_tcp.html#tcp_concepts", null ],
      [ "Tutorial 1: Connecting a Client", "tutorial_tcp.html#tcp_client", null ],
      [ "Tutorial 2: Accepting Connections", "tutorial_tcp.html#tcp_server", null ],
      [ "Tutorial 3: Building an Echo Server", "tutorial_tcp.html#tcp_echo", null ],
      [ "Session Lifetime and Cleanup", "tutorial_tcp.html#tcp_session_lifetime", null ],
      [ "Next Steps", "tutorial_tcp.html#tcp_next", null ]
    ] ],
    [ "Tutorial: WebSocket Chat Application", "tutorial_websocket.html", [
      [ "Introduction", "tutorial_websocket.html#ws_intro", null ],
      [ "Concepts", "tutorial_websocket.html#ws_concepts", null ],
      [ "Tutorial 1: A Broadcasting Chat Server", "tutorial_websocket.html#ws_server", null ],
      [ "Tutorial 2: A Chat Client", "tutorial_websocket.html#ws_client", null ],
      [ "Tutorial 3: Working with Message Framing", "tutorial_websocket.html#ws_framing", null ],
      [ "Next Steps", "tutorial_websocket.html#ws_next", null ]
    ] ],
    [ "Tutorial: Choosing the Right Protocol", "tutorial_protocols.html", [
      [ "Introduction", "tutorial_protocols.html#proto_intro", null ],
      [ "Decision Matrix", "tutorial_protocols.html#proto_matrix", null ],
      [ "Facade Quick Reference", "tutorial_protocols.html#proto_facades", null ],
      [ "Example 1: TCP Reliable Stream", "tutorial_protocols.html#proto_tcp_example", null ],
      [ "Example 2: UDP Datagrams", "tutorial_protocols.html#proto_udp_example", null ],
      [ "Example 3: WebSocket for Browsers", "tutorial_protocols.html#proto_ws_example", null ],
      [ "Swapping Protocols Without Rewriting", "tutorial_protocols.html#proto_swap", null ],
      [ "Next Steps", "tutorial_protocols.html#proto_next", null ]
    ] ],
    [ "Frequently Asked Questions", "faq.html", [
      [ "How do I handle connection drops?", "faq.html#faq_drops", null ],
      [ "How do I configure TLS / SSL?", "faq.html#faq_tls", null ],
      [ "What is the maximum number of concurrent connections?", "faq.html#faq_concurrency", null ],
      [ "How does Network System integrate with container_system?", "faq.html#faq_container", null ],
      [ "What is the threading model?", "faq.html#faq_threads", null ],
      [ "How do I tune buffer sizes?", "faq.html#faq_buffers", null ],
      [ "How do I pick the right protocol?", "faq.html#faq_protocol_choice", null ],
      [ "How do I tune performance?", "faq.html#faq_perf", null ],
      [ "How do I recover from errors safely?", "faq.html#faq_recovery", null ],
      [ "How do I test code that uses Network System?", "faq.html#faq_testing", null ],
      [ "More Resources", "faq.html#faq_more", null ]
    ] ],
    [ "Troubleshooting Guide", "troubleshooting.html", [
      [ "Connection timeouts", "troubleshooting.html#trouble_timeout", null ],
      [ "TLS handshake failures", "troubleshooting.html#trouble_tls", null ],
      [ "Buffer overflow / oversized payloads", "troubleshooting.html#trouble_buffer", null ],
      [ "Memory leaks in long-running connections", "troubleshooting.html#trouble_leak", null ],
      [ "Platform-specific socket issues", "troubleshooting.html#trouble_platform", null ],
      [ "More help", "troubleshooting.html#trouble_more", null ]
    ] ],
    [ "Network System Examples", "md_examples_2README.html", [
      [ "Building", "md_examples_2README.html#autotoc_md641", null ],
      [ "Examples", "md_examples_2README.html#autotoc_md642", [
        [ "tcp_echo_server", "md_examples_2README.html#autotoc_md643", null ],
        [ "tcp_client", "md_examples_2README.html#autotoc_md644", null ],
        [ "websocket_chat", "md_examples_2README.html#autotoc_md645", null ],
        [ "connection_pool", "md_examples_2README.html#autotoc_md646", null ],
        [ "udp_echo", "md_examples_2README.html#autotoc_md647", null ],
        [ "observer_pattern", "md_examples_2README.html#autotoc_md648", null ]
      ] ],
      [ "Key Concepts", "md_examples_2README.html#autotoc_md649", null ],
      [ "API Quick Reference", "md_examples_2README.html#autotoc_md650", [
        [ "Common Pattern", "md_examples_2README.html#autotoc_md651", null ]
      ] ]
    ] ],
    [ "Modules", "modules.html", [
      [ "Modules List", "modules.html", "modules_dup" ],
      [ "Module Members", "modulemembers.html", [
        [ "All", "modulemembers.html", null ],
        [ "Functions", "modulemembers_func.html", null ],
        [ "Variables", "modulemembers_vars.html", null ]
      ] ]
    ] ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", "namespacemembers_dup" ],
        [ "Functions", "namespacemembers_func.html", null ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ]
      ] ]
    ] ],
    [ "Concepts", "concepts.html", "concepts" ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ],
    [ "Examples", "examples.html", "examples" ]
  ] ]
];

var NAVTREEINDEX =
[
"_2home_2runner_2work_2network_system_2network_system_2include_2kcenon_2network_2types_2result_8h-example.html",
"classkcenon_1_1network_1_1core_1_1connection__pool.html#acca2a6694655133bcb66d253d6d8f48d",
"classkcenon_1_1network_1_1core_1_1messaging__client.html#a8bcc8c9d512dfdc3ac05a68a9cfd428d",
"classkcenon_1_1network_1_1core_1_1messaging__quic__client.html#ad57b33b36b6d8e0bbaf46488b0466525",
"classkcenon_1_1network_1_1core_1_1messaging__server.html#a678f4268b6a67cbc7f9ab7c29244fa5c",
"classkcenon_1_1network_1_1core_1_1messaging__udp__client.html#a80e99d6aee34412c54ff83b5f7917bc1",
"classkcenon_1_1network_1_1core_1_1messaging__ws__client.html#a1e41bbe7d1b2248d982e147f3075ff43",
"classkcenon_1_1network_1_1core_1_1network__context.html#a7eb3bc9116e0747f868c65d281684b22",
"classkcenon_1_1network_1_1core_1_1reliable__udp__client_1_1impl.html#a31454267487c74fc86025d4cfb226ee2",
"classkcenon_1_1network_1_1core_1_1secure__messaging__client.html#ab4cd69de769e24367b4dd264fd343934",
"classkcenon_1_1network_1_1core_1_1secure__messaging__server.html#ae6aaebe236ef5d1e5031170eebc653da",
"classkcenon_1_1network_1_1core_1_1secure__messaging__udp__server.html#a172c9bce85018680cc91897e5e699646",
"classkcenon_1_1network_1_1core_1_1session__handle.html#aab91255f8f39b598eedfe1a118a09416",
"classkcenon_1_1network_1_1core_1_1unified__messaging__client.html#adac1ea21d8e9319a562a03bbea9ee65d",
"classkcenon_1_1network_1_1core_1_1unified__session__manager_1_1impl.html#ad46cb421d09ceba50823d67bd6ad9646",
"classkcenon_1_1network_1_1core_1_1ws__connection__impl.html#a9bec4aed799e2dc5d4d3e9dd72b840ce",
"classkcenon_1_1network_1_1integration_1_1ThreadPoolBridge.html#a07bd9c6411fefa349f1348ab03b81f3d",
"classkcenon_1_1network_1_1integration_1_1container__manager.html#af213104f5cd4b8ac2d8d7f7580dcc0ee",
"classkcenon_1_1network_1_1integration_1_1logger__interface.html#ae4fbb4a758bc278f6c94338258da8ed8",
"classkcenon_1_1network_1_1interfaces_1_1i__client.html#a58b3f01ca172647bab5dd7ec0f769f56",
"classkcenon_1_1network_1_1interfaces_1_1i__udp__server.html#a7a3cc4e4685f0dc4adfc14951f2ab02c",
"classkcenon_1_1network_1_1internal_1_1adapters_1_1http__server__adapter.html#aa7f9ceafb34b2c7d750ef29323244674",
"classkcenon_1_1network_1_1internal_1_1adapters_1_1udp__client__adapter.html#a07344370d552d147cb8439c29f2dda3b",
"classkcenon_1_1network_1_1internal_1_1adapters_1_1ws__session__wrapper.html#a2289411763f377dc5f95814c640872fb",
"classkcenon_1_1network_1_1internal_1_1quic__socket.html#acd1d4ef257f67db758914012765b83c7",
"classkcenon_1_1network_1_1internal_1_1websocket__handshake.html#ad099e35418d824481f97a93afbe38551",
"classkcenon_1_1network_1_1metrics_1_1metric__reporter.html#aec2b1da7301f8528f4eb74276492523d",
"classkcenon_1_1network_1_1protocols_1_1grpc_1_1grpc__client_1_1bidi__stream.html#a3fc4f8d13137e8659c1e0a9ed9560f37",
"classkcenon_1_1network_1_1protocols_1_1grpc_1_1server__stream__reader__impl.html#a617661adda0dae9a47f77c466d650cae",
"classkcenon_1_1network_1_1protocols_1_1http2_1_1http2__client.html#a0d473e7f4b8cc26748748d54878eb4c4",
"classkcenon_1_1network_1_1protocols_1_1http2_1_1http2__server__connection.html#a1e53748a7ac6e1bd26f9628532bc1229",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1congestion__controller.html#ae378a2eb881801de64efba5eaf189ef2",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1connection__id.html#ab1b95d9e77dd9e8a6959fa07fe341b59",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1frame__parser.html#ae08e609948528a44330a4f7deb609ae2",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1quic__crypto.html#a90e5a53449f14d5c1560283515b5d1ef",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1stream__manager.html#a00dffd2252c81743a418fa8d32b523c8",
"classkcenon_1_1network_1_1session_1_1quic__session.html#a62d8ad2c840f013c4accd3f3ef90b4b9",
"classkcenon_1_1network_1_1unified_1_1adapters_1_1quic__connection__adapter.html#a31d856eb11a6f3d863fa1d8ed7975ce2",
"classkcenon_1_1network_1_1unified_1_1adapters_1_1tcp__listener__adapter.html#aa4372040b9d47574a5bf01402550119a",
"classkcenon_1_1network_1_1unified_1_1adapters_1_1ws__listener__adapter.html#ac0ff20710c739d8036a03777da72e1eb",
"classkcenon_1_1network_1_1utils_1_1connection__state.html#a7fff52ff3918e0c484ea05139589cdbb",
"compression__pipeline_8cpp_source.html",
"ecn__tracker_8h.html#a9d5a7df28dcb5135985569f29069a277a92a0c1a21c943d12584ed3721b896258",
"http__error_8h.html#a86204ce59ef1afbff2a069cdfe73bae2a34b1f5a8577eed2180e10076d1aad14c",
"md_README.html#autotoc_md589",
"module__kcenon_8network.html#a7aca2c76688d54be3cc6451da7cc74e1",
"namespacekcenon_1_1network.html#ad9036e44065f7a7f53fae87333893985",
"namespacekcenon_1_1network_1_1internal.html#af5a510a33e8abe4e4f14dd4a56dd8131a2d2c8732a55a4f0194b397eda886dbb5",
"namespacekcenon_1_1network_1_1protocols_1_1quic.html#a5c1d3dd2e50e350483f170eaa2ccffd3a8f3f6bf3f291ae87be5963ec05393bdb",
"network__metrics_8h.html#a4429b5138d491f8733d1dd6971f28a29",
"secure__messaging__udp__client_8cpp.html",
"structkcenon_1_1network_1_1config_1_1network__config.html",
"structkcenon_1_1network_1_1core_1_1session__info.html#aa83f29162ec7876802872ec72a5dd8f8",
"structkcenon_1_1network_1_1events_1_1network__latency__event.html",
"structkcenon_1_1network_1_1integration_1_1io__context__thread__manager_1_1impl_1_1context__entry.html",
"structkcenon_1_1network_1_1internal_1_1ws__frame__header.html#a715fae7dcc94b62a5540d9b6ac97e04d",
"structkcenon_1_1network_1_1protocols_1_1grpc_1_1grpc__server__config.html",
"structkcenon_1_1network_1_1protocols_1_1quic_1_1connection__close__frame.html#a2fb1355aa0531156c131a7e835ab2eb8",
"structkcenon_1_1network_1_1protocols_1_1quic_1_1quic__crypto_1_1impl.html#ad190ad169b8ae0d5cd509ede8aed5929",
"structkcenon_1_1network_1_1tracing_1_1span_1_1impl.html#a0aad5e1d59746943f5dd32d32df5bde9",
"trace__context_8h.html#a62aa701deab59b8d974ad176543641dd",
"websocket__socket_8h.html#aaab5e8a7ac9e59e19eac7039dea3050da349e686330723975502e9ef4f939a5ac"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';