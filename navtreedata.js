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
        [ "", "md_README.html#autotoc_md557", null ],
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
          [ "Protocol Support Status", "md_README.html#autotoc_md578", null ],
          [ "Dependency Graph", "md_README.html#autotoc_md579", null ],
          [ "Selective Linking", "md_README.html#autotoc_md580", null ],
          [ "Umbrella Header", "md_README.html#autotoc_md581", null ]
        ] ],
        [ "Core Features", "md_README.html#autotoc_md583", [
          [ "Protocols", "md_README.html#autotoc_md584", null ],
          [ "Distributed Tracing", "md_README.html#autotoc_md585", null ],
          [ "Asynchronous Model", "md_README.html#autotoc_md586", null ],
          [ "Failure Handling", "md_README.html#autotoc_md587", null ],
          [ "Error Handling", "md_README.html#autotoc_md588", null ]
        ] ],
        [ "Performance Highlights", "md_README.html#autotoc_md590", [
          [ "Synthetic Benchmarks (Intel i7-12700K, Ubuntu 22.04, GCC 11, -O3)", "md_README.html#autotoc_md591", null ],
          [ "Real I/O Benchmarks (Loopback TCP)", "md_README.html#autotoc_md592", null ],
          [ "Reproducing Benchmarks", "md_README.html#autotoc_md593", null ]
        ] ],
        [ "Architecture Overview", "md_README.html#autotoc_md595", null ],
        [ "Ecosystem Integration", "md_README.html#autotoc_md597", [
          [ "Ecosystem Dependency Map", "md_README.html#autotoc_md598", null ],
          [ "Related Projects", "md_README.html#autotoc_md599", null ],
          [ "Integration Example", "md_README.html#autotoc_md600", null ],
          [ "Thread Pool Adapters", "md_README.html#autotoc_md601", null ]
        ] ],
        [ "Documentation", "md_README.html#autotoc_md603", [
          [ "Getting Started", "md_README.html#autotoc_md604", [
            [ "Generated API Docs (Doxygen)", "md_README.html#autotoc_md605", null ]
          ] ],
          [ "Advanced Topics", "md_README.html#autotoc_md606", null ],
          [ "Development", "md_README.html#autotoc_md607", null ]
        ] ],
        [ "Platform Support", "md_README.html#autotoc_md609", null ],
        [ "Production Quality", "md_README.html#autotoc_md611", [
          [ "CI/CD Infrastructure", "md_README.html#autotoc_md612", null ],
          [ "Security", "md_README.html#autotoc_md613", null ],
          [ "Thread Safety & Memory Safety", "md_README.html#autotoc_md614", null ]
        ] ],
        [ "Dependencies", "md_README.html#autotoc_md616", [
          [ "Required", "md_README.html#autotoc_md617", null ]
        ] ],
        [ "", "md_README.html#autotoc_md618", null ],
        [ "Build Options", "md_README.html#autotoc_md619", [
          [ "Using CMake Presets (Recommended)", "md_README.html#autotoc_md620", null ],
          [ "Manual CMake Configuration", "md_README.html#autotoc_md621", null ],
          [ "Cleaning Build Directories", "md_README.html#autotoc_md622", null ],
          [ "Available CMake Options", "md_README.html#autotoc_md623", null ]
        ] ],
        [ "Examples", "md_README.html#autotoc_md625", null ],
        [ "Roadmap", "md_README.html#autotoc_md627", [
          [ "Recently Completed", "md_README.html#autotoc_md628", null ],
          [ "Current Focus", "md_README.html#autotoc_md629", null ],
          [ "Planned Features", "md_README.html#autotoc_md630", null ]
        ] ],
        [ "Contributing", "md_README.html#autotoc_md632", null ],
        [ "License", "md_README.html#autotoc_md634", null ],
        [ "Contact & Support", "md_README.html#autotoc_md636", null ],
        [ "Acknowledgments", "md_README.html#autotoc_md638", [
          [ "Core Dependencies", "md_README.html#autotoc_md639", null ],
          [ "Ecosystem Integration", "md_README.html#autotoc_md640", null ]
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
    [ "Migration Examples", "md_examples_2migration_2README.html", [
      [ "Files", "md_examples_2migration_2README.html#autotoc_md643", null ],
      [ "Quick Comparison", "md_examples_2migration_2README.html#autotoc_md644", [
        [ "Client Code", "md_examples_2migration_2README.html#autotoc_md645", null ],
        [ "Server Code", "md_examples_2migration_2README.html#autotoc_md646", null ]
      ] ],
      [ "Key Benefits", "md_examples_2migration_2README.html#autotoc_md647", null ],
      [ "Building Examples", "md_examples_2migration_2README.html#autotoc_md648", null ],
      [ "See Also", "md_examples_2migration_2README.html#autotoc_md649", null ]
    ] ],
    [ "Network System Examples", "md_examples_2README.html", [
      [ "Building", "md_examples_2README.html#autotoc_md651", null ],
      [ "Examples", "md_examples_2README.html#autotoc_md652", [
        [ "tcp_echo_server", "md_examples_2README.html#autotoc_md653", null ],
        [ "tcp_client", "md_examples_2README.html#autotoc_md654", null ],
        [ "websocket_chat", "md_examples_2README.html#autotoc_md655", null ],
        [ "connection_pool", "md_examples_2README.html#autotoc_md656", null ],
        [ "udp_echo", "md_examples_2README.html#autotoc_md657", null ],
        [ "observer_pattern", "md_examples_2README.html#autotoc_md658", null ],
        [ "basic_usage", "md_examples_2README.html#autotoc_md659", null ],
        [ "concepts_example", "md_examples_2README.html#autotoc_md660", null ],
        [ "memory_profile_demo", "md_examples_2README.html#autotoc_md661", null ],
        [ "simple_http_client / simple_http_server", "md_examples_2README.html#autotoc_md662", null ],
        [ "http_facade_example", "md_examples_2README.html#autotoc_md663", null ],
        [ "http2_server_example", "md_examples_2README.html#autotoc_md664", null ],
        [ "quic_client_example / quic_server_example", "md_examples_2README.html#autotoc_md665", null ],
        [ "quic_facade_example", "md_examples_2README.html#autotoc_md666", null ],
        [ "grpc_service_example", "md_examples_2README.html#autotoc_md667", null ],
        [ "tls_policy_example", "md_examples_2README.html#autotoc_md668", null ],
        [ "thread_integration_example / network_metrics_example / network_tracing_example", "md_examples_2README.html#autotoc_md669", null ],
        [ "unified_messaging_example", "md_examples_2README.html#autotoc_md670", null ],
        [ "messaging_system_integration / migration", "md_examples_2README.html#autotoc_md671", null ]
      ] ],
      [ "Key Concepts", "md_examples_2README.html#autotoc_md672", null ],
      [ "API Quick Reference", "md_examples_2README.html#autotoc_md673", [
        [ "Common Pattern", "md_examples_2README.html#autotoc_md674", null ]
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
"classkcenon_1_1network_1_1internal_1_1quic__socket.html#a237ae77aa1042fe3efb42e21499af112",
"classkcenon_1_1network_1_1internal_1_1tcp__socket.html#accbf3a7772f87872a1fa12b81019b271",
"classkcenon_1_1network_1_1metrics_1_1histogram.html#a115ec09d81481a51a07d36e28e9c8518",
"classkcenon_1_1network_1_1protocols_1_1grpc_1_1generic__service.html#a6080c4677f15dcf3b28ba2b99c5a5184",
"classkcenon_1_1network_1_1protocols_1_1grpc_1_1grpc__server_1_1impl.html#aee977b871b4b5c46c825e7829266795f",
"classkcenon_1_1network_1_1protocols_1_1http2_1_1frame.html#a4021592f8f32bea6ed64fd080acec34b",
"classkcenon_1_1network_1_1protocols_1_1http2_1_1http2__server.html#a3b42cfc8c70cc8b94c6ede1701d13134",
"classkcenon_1_1network_1_1protocols_1_1http2_1_1http2__server__stream.html#acd11990818d0638b3e22c40f4271b2dd",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1connection.html#a82794cc00b02dbade206445d6b60e71d",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1flow__controller.html#a612d67e2738f4079db37cdbf41a592bb",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1packet__protection.html#a08a9cdd7592cb5bf263322cb1b5490f1",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1stream.html#a0cdd6c72e655173ec6384eebfc3890fa",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1varint.html#ad42f07a3ede1c3bf9d8f1acf04fe291a",
"classkcenon_1_1network_1_1tracing_1_1span.html#a588626bda7da917dda96b7ed813c5a9b",
"classkcenon_1_1network_1_1unified_1_1adapters_1_1quic__listener__adapter.html#adc61d85eff8b9258482ee22917eff468",
"classkcenon_1_1network_1_1unified_1_1adapters_1_1udp__listener__adapter.html#ad278865c4bd61c79458a8873d7029833",
"classkcenon_1_1network_1_1utils_1_1buffer__pool.html#ab738943ddfae4424f12dab6528126ade",
"classkcenon_1_1network_1_1utils_1_1memory__profiler.html#a7e61ae583a0816199920023ea565480b",
"connection_8h.html#a300c24cbdc0671e75b37d2d8e3ce0dd9a06aa6fa8bdc2078e7e1bd903e70c8f6a",
"frame__types_8h.html#a9e731245c02bcbbaeef17082ee32d19b",
"http__types_8h.html#a23bcbc57fff48e58c123a7fc997fa5e4",
"md_include_2kcenon_2network_2core_2README.html#autotoc_md1",
"module__kcenon_8network.html#a907626d622c668028a3a3a84a386d4dba334c4a4c42fdb79d7ebc3e73b517e6f8",
"namespacekcenon_1_1network_1_1core.html#a4f0323124f9a81ca2e751ef3f65c0128acb538d2ef0cedf41c6107cd512374cb1",
"namespacekcenon_1_1network_1_1protocol_1_1quic.html#a15d3e9736fff910f400b88fe2a14fcd0",
"namespacekcenon_1_1network_1_1protocols_1_1quic.html#ac85c4a16a8266f06be2865cb523fc2a4ad9a22d7a8178d5b42a8750123cbfe5b1",
"network__system__bridge_8cpp_source.html",
"server_8cpp.html#a447099b62e1bc9677eea2324e68092daa1bb4e700c4106a28f86525f93fc90ded",
"structkcenon_1_1network_1_1config_1_1network__system__config.html",
"structkcenon_1_1network_1_1core_1_1session__info__base_3_01SessionType_00_01true_01_4.html#a4eae949e3a20bbf6ad7b03dabde3c5cd",
"structkcenon_1_1network_1_1events_1_1network__latency__event.html#a90f624526471ee003752a27c3820ad40",
"structkcenon_1_1network_1_1integration_1_1io__context__thread__manager_1_1metrics.html#a85bce4e452d6a4895625219ad1d41a37",
"structkcenon_1_1network_1_1internal_1_1ws__message.html",
"structkcenon_1_1network_1_1protocols_1_1grpc_1_1grpc__server__config.html#af28258c0b9d3a73eb986594745891d4e",
"structkcenon_1_1network_1_1protocols_1_1quic_1_1connection__id__entry.html",
"structkcenon_1_1network_1_1protocols_1_1quic_1_1quic__crypto_1_1impl.html#aede92b0566aa78bed5a18d46926fb0bd",
"structkcenon_1_1network_1_1tracing_1_1span_1_1impl.html#a1906511f03d751b90f8c7aa69dc0914c",
"trace__context_8h.html#acaade1b1174118e563575ac5c4223c64",
"websocket__server_8cpp_source.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';