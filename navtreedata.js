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
    [ "Network System", "md_README.html", [
      [ "Overview", "md_README.html#autotoc_md554", null ],
      [ "Contents", "md_README.html#autotoc_md555", null ],
      [ "Requirements", "md_README.html#autotoc_md556", null ],
      [ "Getting Started", "md_README.html#autotoc_md557", null ],
      [ "Documentation", "md_README.html#autotoc_md558", null ],
      [ "Development and Support", "md_README.html#autotoc_md559", null ],
      [ "License", "md_README.html#autotoc_md560", null ]
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
    [ "Migration Examples", "md_examples_2migration_2README.html", [
      [ "Files", "md_examples_2migration_2README.html#autotoc_md562", null ],
      [ "Quick Comparison", "md_examples_2migration_2README.html#autotoc_md563", [
        [ "Client Code", "md_examples_2migration_2README.html#autotoc_md564", null ],
        [ "Server Code", "md_examples_2migration_2README.html#autotoc_md565", null ]
      ] ],
      [ "Key Benefits", "md_examples_2migration_2README.html#autotoc_md566", null ],
      [ "Building Examples", "md_examples_2migration_2README.html#autotoc_md567", null ],
      [ "See Also", "md_examples_2migration_2README.html#autotoc_md568", null ]
    ] ],
    [ "Network System Examples", "md_examples_2README.html", [
      [ "Building", "md_examples_2README.html#autotoc_md570", null ],
      [ "Examples", "md_examples_2README.html#autotoc_md571", [
        [ "tcp_echo_server", "md_examples_2README.html#autotoc_md572", null ],
        [ "tcp_client", "md_examples_2README.html#autotoc_md573", null ],
        [ "websocket_chat", "md_examples_2README.html#autotoc_md574", null ],
        [ "connection_pool", "md_examples_2README.html#autotoc_md575", null ],
        [ "udp_echo", "md_examples_2README.html#autotoc_md576", null ],
        [ "observer_pattern", "md_examples_2README.html#autotoc_md577", null ],
        [ "basic_usage", "md_examples_2README.html#autotoc_md578", null ],
        [ "concepts_example", "md_examples_2README.html#autotoc_md579", null ],
        [ "memory_profile_demo", "md_examples_2README.html#autotoc_md580", null ],
        [ "simple_http_client / simple_http_server", "md_examples_2README.html#autotoc_md581", null ],
        [ "http_facade_example", "md_examples_2README.html#autotoc_md582", null ],
        [ "http2_server_example", "md_examples_2README.html#autotoc_md583", null ],
        [ "quic_client_example / quic_server_example", "md_examples_2README.html#autotoc_md584", null ],
        [ "quic_facade_example", "md_examples_2README.html#autotoc_md585", null ],
        [ "grpc_service_example", "md_examples_2README.html#autotoc_md586", null ],
        [ "tls_policy_example", "md_examples_2README.html#autotoc_md587", null ],
        [ "thread_integration_example / network_metrics_example / network_tracing_example", "md_examples_2README.html#autotoc_md588", null ],
        [ "unified_messaging_example", "md_examples_2README.html#autotoc_md589", null ],
        [ "messaging_system_integration / migration", "md_examples_2README.html#autotoc_md590", null ]
      ] ],
      [ "Key Concepts", "md_examples_2README.html#autotoc_md591", null ],
      [ "API Quick Reference", "md_examples_2README.html#autotoc_md592", [
        [ "Common Pattern", "md_examples_2README.html#autotoc_md593", null ]
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
"_2home_2runner_2work_2network_system_2network_system_2src_2internal_2utils_2message_validator_8h-example.html",
"classkcenon_1_1network_1_1connection__limiter.html#ab091110dc45ab233a2d4d206613ff2d9",
"classkcenon_1_1network_1_1core_1_1messaging__client.html#a3b524633483c6ecfd9b1db0a2eaf1b6e",
"classkcenon_1_1network_1_1core_1_1messaging__quic__client.html#a53b51970802f4b9c82a8da8a232aa51b",
"classkcenon_1_1network_1_1core_1_1messaging__server.html#a0d298a47a6ee6b53691ebbe9d60b8f7a",
"classkcenon_1_1network_1_1core_1_1messaging__udp__client.html#a1e6b3106042ec2ff92983bd0c6f30fd8",
"classkcenon_1_1network_1_1core_1_1messaging__udp__server.html#a8a9486bf7681179d53c24dc9822ace04",
"classkcenon_1_1network_1_1core_1_1messaging__ws__server.html#ac81a58b350ff3f899d9655465beae885",
"classkcenon_1_1network_1_1core_1_1reliable__udp__client.html#a8d92c9cd68dcaedf5cf34cbb6efb2036",
"classkcenon_1_1network_1_1core_1_1secure__messaging__client.html#a677f040ea15982715900791d113cc5b8",
"classkcenon_1_1network_1_1core_1_1secure__messaging__server.html#a8a3e2e10d0fdd1f0bb144103777bd16f",
"classkcenon_1_1network_1_1core_1_1secure__messaging__udp__client.html#ac1af03ea6ba0b4dedb6dbba39bae1cfc",
"classkcenon_1_1network_1_1core_1_1session__concept.html#a2edd0c056e7346e95fc38368d6c55572",
"classkcenon_1_1network_1_1core_1_1unified__messaging__client.html#a3827e30d2cbf3aeb1262bf4819a2c447",
"classkcenon_1_1network_1_1core_1_1unified__session__manager.html#aca930435e12be105f3a618793dd55947",
"classkcenon_1_1network_1_1core_1_1unified__udp__messaging__server.html#ac5339f9598a920a503b32be6a2c51f82",
"classkcenon_1_1network_1_1integration_1_1NetworkSystemBridge_1_1Impl.html#a3b87057df867bd99c9908b7e3f10a61b",
"classkcenon_1_1network_1_1integration_1_1basic__thread__pool_1_1impl.html#a09afc814ba520724cfe747c67d140e59",
"classkcenon_1_1network_1_1integration_1_1io__context__thread__manager_1_1impl.html#a8cd868a89045a5185ce9ec6fdfbadde5",
"classkcenon_1_1network_1_1integration_1_1thread__pool__interface.html#accaabec524d93d8a782df81c994b6a70",
"classkcenon_1_1network_1_1interfaces_1_1i__server.html",
"classkcenon_1_1network_1_1internal_1_1adapters_1_1http__request__session.html#a3234309336af359398d2cf0d5d058eb3",
"classkcenon_1_1network_1_1internal_1_1adapters_1_1quic__session__wrapper.html",
"classkcenon_1_1network_1_1internal_1_1adapters_1_1ws__client__adapter.html#adc83f9eef2a92a5b024e83babe1527cc",
"classkcenon_1_1network_1_1internal_1_1quic__socket.html#a2c086fb725d6189ff1265325c738b0c2",
"classkcenon_1_1network_1_1internal_1_1tcp__socket.html#a9823197f687cc425f2e141a02a16eba8",
"classkcenon_1_1network_1_1message__validator.html#a83ca10149a178460b254c497a37aa03d",
"classkcenon_1_1network_1_1protocols_1_1grpc_1_1client__stream__writer__impl.html#aaec0b33bd821cba64fe7595575270a25",
"classkcenon_1_1network_1_1protocols_1_1grpc_1_1grpc__server_1_1impl.html#a862729b56c9763c52d54a62c7af7a1d1",
"classkcenon_1_1network_1_1protocols_1_1http2_1_1dynamic__table.html#aaeeebf9ed29c413974c48fd7311b0cda",
"classkcenon_1_1network_1_1protocols_1_1http2_1_1http2__server.html#a02fa5acdab0b5c96afeea1ec7b0960d7",
"classkcenon_1_1network_1_1protocols_1_1http2_1_1http2__server__stream.html#a8de5f7865c3f87aa48ef9697f5ad1243",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1connection.html#a720109c56cf29b75226d00b1863ad939",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1flow__controller.html#a420adbeea7e4d601cd56f72106e5cb6a",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1packet__parser.html#a03a95c6b2ddc5734b186ad081e94e203",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1session__ticket__store.html#ab8cd880ffb5249fba33b67842ac94527",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1varint.html#a8d95e6a90e4df550528c222198bf8acb",
"classkcenon_1_1network_1_1tracing_1_1span.html#a1d5745a71e99ab427a865efb7098eddb",
"classkcenon_1_1network_1_1unified_1_1adapters_1_1quic__listener__adapter.html#ab91b1464b33a63b74a6fb7aa4cf96720",
"classkcenon_1_1network_1_1unified_1_1adapters_1_1udp__listener__adapter.html#a881c0d3df485b7a0251331f111415621",
"classkcenon_1_1network_1_1unified_1_1i__transport.html#af72b81befc378e6938f1ab49ea016946",
"classkcenon_1_1network_1_1utils_1_1memory__profiler.html#a54373d118dcd207c5844401701618908",
"congestion__controller_8cpp.html#a5c95591dd2426688d8e6edc5ee8b23d1",
"frame__types_8h.html#a32af302166a80bcd687c7d6895502defa90493a7e859a7f2e045b526283290e69",
"http__server_8h.html",
"messaging__bridge_8h.html#a6caa3102111e2bc0cc9aabce3a65ca96",
"module__kcenon_8network.html#ab7b229e4d188d5ada96babe578dacc61",
"namespacekcenon_1_1network_1_1error__codes_1_1common__errors.html#aef01a234bf49a736c598497052a45829",
"namespacekcenon_1_1network_1_1protocols_1_1grpc.html#a447099b62e1bc9677eea2324e68092daa78b68caae6b86a022e96803118c94ce8",
"namespacekcenon_1_1network_1_1protocols_1_1quic_1_1stream__error.html#aec58bd655e22a424778592e69185ab4d",
"pmtud__controller_8cpp_source.html",
"session__manager_8h_source.html",
"structkcenon_1_1network_1_1core_1_1http__request__context.html#a9162fc55508ac23072b873b9f031b024",
"structkcenon_1_1network_1_1core_1_1session__model_1_1has__send__method_3_01T_00_01std_1_1void__t14620cbbe17ed3e4038ff148cc7c854e.html",
"structkcenon_1_1network_1_1events_1_1network__transfer__event.html#acd5b955a9b980f2722f6b748ab52ba8a",
"structkcenon_1_1network_1_1internal_1_1cookie.html#a0e8187a750fd611e7bd8e2fe364082e6",
"structkcenon_1_1network_1_1message__limits.html#aff027d1578816a855933770b22fd4c11",
"structkcenon_1_1network_1_1protocols_1_1grpc_1_1grpc__status.html#af98194e483588528f2bc25dee0376688",
"structkcenon_1_1network_1_1protocols_1_1quic_1_1ecn__counts.html#ac8f08014b030bef7516fe49171f920c9",
"structkcenon_1_1network_1_1protocols_1_1quic_1_1replay__filter_1_1config.html",
"structkcenon_1_1network_1_1tracing_1_1span__event.html",
"tracing__config_8h.html#ae21ebffb04b71c149ad289b10e26bbf6",
"ws__listener__adapter_8cpp.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';