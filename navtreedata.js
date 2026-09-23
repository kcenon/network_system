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
    [ "Network System Examples", "md_examples_2README.html", [
      [ "Building", "md_examples_2README.html#autotoc_md643", null ],
      [ "Examples", "md_examples_2README.html#autotoc_md644", [
        [ "tcp_echo_server", "md_examples_2README.html#autotoc_md645", null ],
        [ "tcp_client", "md_examples_2README.html#autotoc_md646", null ],
        [ "websocket_chat", "md_examples_2README.html#autotoc_md647", null ],
        [ "connection_pool", "md_examples_2README.html#autotoc_md648", null ],
        [ "udp_echo", "md_examples_2README.html#autotoc_md649", null ],
        [ "observer_pattern", "md_examples_2README.html#autotoc_md650", null ]
      ] ],
      [ "Key Concepts", "md_examples_2README.html#autotoc_md651", null ],
      [ "API Quick Reference", "md_examples_2README.html#autotoc_md652", [
        [ "Common Pattern", "md_examples_2README.html#autotoc_md653", null ]
      ] ]
    ] ],
    [ "Migration Examples", "md_examples_2tutorials_2migration_2README.html", [
      [ "Files", "md_examples_2tutorials_2migration_2README.html#autotoc_md655", null ],
      [ "Quick Comparison", "md_examples_2tutorials_2migration_2README.html#autotoc_md656", [
        [ "Client Code", "md_examples_2tutorials_2migration_2README.html#autotoc_md657", null ],
        [ "Server Code", "md_examples_2tutorials_2migration_2README.html#autotoc_md658", null ]
      ] ],
      [ "Key Benefits", "md_examples_2tutorials_2migration_2README.html#autotoc_md659", null ],
      [ "Building Examples", "md_examples_2tutorials_2migration_2README.html#autotoc_md660", null ],
      [ "See Also", "md_examples_2tutorials_2migration_2README.html#autotoc_md661", null ]
    ] ],
    [ "Network System Samples", "md_examples_2tutorials_2README.html", [
      [ "Latest Updates (2025-10-10)", "md_examples_2tutorials_2README.html#autotoc_md663", null ],
      [ "Available Samples", "md_examples_2tutorials_2README.html#autotoc_md664", [
        [ "Basic Usage (basic_usage.cpp)", "md_examples_2tutorials_2README.html#autotoc_md665", null ],
        [ "TCP Server/Client Demo (tcp_server_client.cpp)", "md_examples_2tutorials_2README.html#autotoc_md666", null ],
        [ "HTTP Client Demo (http_client_demo.cpp)", "md_examples_2tutorials_2README.html#autotoc_md667", null ],
        [ "Modern Usage (messaging_system_integration/modern_usage.cpp)", "md_examples_2tutorials_2README.html#autotoc_md668", null ],
        [ "Run All Samples (run_all_samples.cpp)", "md_examples_2tutorials_2README.html#autotoc_md669", null ]
      ] ],
      [ "Building the Samples", "md_examples_2tutorials_2README.html#autotoc_md670", [
        [ "Prerequisites", "md_examples_2tutorials_2README.html#autotoc_md671", null ],
        [ "Build Instructions", "md_examples_2tutorials_2README.html#autotoc_md672", null ],
        [ "Alternative Build (samples only)", "md_examples_2tutorials_2README.html#autotoc_md673", null ]
      ] ],
      [ "Network Configuration", "md_examples_2tutorials_2README.html#autotoc_md674", [
        [ "TCP Server Configuration", "md_examples_2tutorials_2README.html#autotoc_md675", null ],
        [ "HTTP Client Configuration", "md_examples_2tutorials_2README.html#autotoc_md676", null ],
        [ "Customizing Configuration", "md_examples_2tutorials_2README.html#autotoc_md677", null ]
      ] ],
      [ "Sample Output Examples", "md_examples_2tutorials_2README.html#autotoc_md678", [
        [ "Basic Usage Output", "md_examples_2tutorials_2README.html#autotoc_md679", null ],
        [ "TCP Server/Client Demo Output", "md_examples_2tutorials_2README.html#autotoc_md680", null ],
        [ "HTTP Client Demo Output", "md_examples_2tutorials_2README.html#autotoc_md681", null ]
      ] ],
      [ "Understanding the Results", "md_examples_2tutorials_2README.html#autotoc_md682", [
        [ "Performance Metrics", "md_examples_2tutorials_2README.html#autotoc_md683", null ],
        [ "TCP Communication", "md_examples_2tutorials_2README.html#autotoc_md684", null ],
        [ "HTTP Operations", "md_examples_2tutorials_2README.html#autotoc_md685", null ]
      ] ],
      [ "Result<T> Error Handling Pattern", "md_examples_2tutorials_2README.html#autotoc_md686", [
        [ "Type-Safe Error Checking", "md_examples_2tutorials_2README.html#autotoc_md687", null ],
        [ "Error Code Categories", "md_examples_2tutorials_2README.html#autotoc_md688", null ],
        [ "Error Recovery Patterns", "md_examples_2tutorials_2README.html#autotoc_md689", null ]
      ] ],
      [ "Advanced Usage", "md_examples_2tutorials_2README.html#autotoc_md690", [
        [ "Custom TCP Protocol", "md_examples_2tutorials_2README.html#autotoc_md691", null ],
        [ "HTTP Client Extensions", "md_examples_2tutorials_2README.html#autotoc_md692", null ],
        [ "Concurrent Server Pattern", "md_examples_2tutorials_2README.html#autotoc_md693", null ]
      ] ],
      [ "Troubleshooting", "md_examples_2tutorials_2README.html#autotoc_md694", [
        [ "Common Issues", "md_examples_2tutorials_2README.html#autotoc_md695", null ],
        [ "Platform-Specific Considerations", "md_examples_2tutorials_2README.html#autotoc_md696", [
          [ "Windows", "md_examples_2tutorials_2README.html#autotoc_md697", null ],
          [ "Linux", "md_examples_2tutorials_2README.html#autotoc_md698", null ],
          [ "macOS", "md_examples_2tutorials_2README.html#autotoc_md699", null ]
        ] ],
        [ "Performance Optimization", "md_examples_2tutorials_2README.html#autotoc_md700", null ],
        [ "Testing Network Features", "md_examples_2tutorials_2README.html#autotoc_md701", [
          [ "Local Testing", "md_examples_2tutorials_2README.html#autotoc_md702", null ],
          [ "Network Testing", "md_examples_2tutorials_2README.html#autotoc_md703", null ]
        ] ]
      ] ],
      [ "Security Considerations", "md_examples_2tutorials_2README.html#autotoc_md704", [
        [ "Network Security", "md_examples_2tutorials_2README.html#autotoc_md705", null ],
        [ "Error Handling", "md_examples_2tutorials_2README.html#autotoc_md706", null ]
      ] ],
      [ "Extension Points", "md_examples_2tutorials_2README.html#autotoc_md707", [
        [ "Adding New Samples", "md_examples_2tutorials_2README.html#autotoc_md708", null ],
        [ "Custom Protocols", "md_examples_2tutorials_2README.html#autotoc_md709", null ],
        [ "Integration Examples", "md_examples_2tutorials_2README.html#autotoc_md710", null ]
      ] ],
      [ "Getting Help", "md_examples_2tutorials_2README.html#autotoc_md711", null ],
      [ "License", "md_examples_2tutorials_2README.html#autotoc_md712", null ]
    ] ],
    [ "Network System 샘플", "md_examples_2tutorials_2README__KO.html", [
      [ "최신 업데이트 (2025-10-10)", "md_examples_2tutorials_2README__KO.html#autotoc_md714", null ],
      [ "사용 가능한 샘플", "md_examples_2tutorials_2README__KO.html#autotoc_md715", [
        [ "기본 사용법 (basic_usage.cpp)", "md_examples_2tutorials_2README__KO.html#autotoc_md716", null ],
        [ "TCP 서버/클라이언트 데모 (tcp_server_client.cpp)", "md_examples_2tutorials_2README__KO.html#autotoc_md717", null ],
        [ "HTTP 클라이언트 데모 (http_client_demo.cpp)", "md_examples_2tutorials_2README__KO.html#autotoc_md718", null ],
        [ "현대적 사용법 (messaging_system_integration/modern_usage.cpp)", "md_examples_2tutorials_2README__KO.html#autotoc_md719", null ],
        [ "모든 샘플 실행 (run_all_samples.cpp)", "md_examples_2tutorials_2README__KO.html#autotoc_md720", null ]
      ] ],
      [ "샘플 빌드", "md_examples_2tutorials_2README__KO.html#autotoc_md721", [
        [ "필수 요구사항", "md_examples_2tutorials_2README__KO.html#autotoc_md722", null ],
        [ "빌드 지침", "md_examples_2tutorials_2README__KO.html#autotoc_md723", null ],
        [ "대체 빌드 (샘플만)", "md_examples_2tutorials_2README__KO.html#autotoc_md724", null ]
      ] ],
      [ "네트워크 구성", "md_examples_2tutorials_2README__KO.html#autotoc_md725", [
        [ "TCP 서버 구성", "md_examples_2tutorials_2README__KO.html#autotoc_md726", null ],
        [ "HTTP 클라이언트 구성", "md_examples_2tutorials_2README__KO.html#autotoc_md727", null ],
        [ "구성 사용자 정의", "md_examples_2tutorials_2README__KO.html#autotoc_md728", null ]
      ] ],
      [ "샘플 출력 예제", "md_examples_2tutorials_2README__KO.html#autotoc_md729", [
        [ "기본 사용법 출력", "md_examples_2tutorials_2README__KO.html#autotoc_md730", null ],
        [ "TCP 서버/클라이언트 데모 출력", "md_examples_2tutorials_2README__KO.html#autotoc_md731", null ],
        [ "HTTP 클라이언트 데모 출력", "md_examples_2tutorials_2README__KO.html#autotoc_md732", null ]
      ] ],
      [ "결과 이해", "md_examples_2tutorials_2README__KO.html#autotoc_md733", [
        [ "성능 지표", "md_examples_2tutorials_2README__KO.html#autotoc_md734", null ],
        [ "TCP 통신", "md_examples_2tutorials_2README__KO.html#autotoc_md735", null ],
        [ "HTTP 작업", "md_examples_2tutorials_2README__KO.html#autotoc_md736", null ]
      ] ],
      [ "Result<T> 오류 처리 패턴", "md_examples_2tutorials_2README__KO.html#autotoc_md737", [
        [ "타입 안전 오류 확인", "md_examples_2tutorials_2README__KO.html#autotoc_md738", null ],
        [ "오류 코드 카테고리", "md_examples_2tutorials_2README__KO.html#autotoc_md739", null ],
        [ "오류 복구 패턴", "md_examples_2tutorials_2README__KO.html#autotoc_md740", null ]
      ] ],
      [ "고급 사용법", "md_examples_2tutorials_2README__KO.html#autotoc_md741", [
        [ "사용자 정의 TCP 프로토콜", "md_examples_2tutorials_2README__KO.html#autotoc_md742", null ],
        [ "HTTP 클라이언트 확장", "md_examples_2tutorials_2README__KO.html#autotoc_md743", null ],
        [ "동시 서버 패턴", "md_examples_2tutorials_2README__KO.html#autotoc_md744", null ]
      ] ],
      [ "문제 해결", "md_examples_2tutorials_2README__KO.html#autotoc_md745", [
        [ "일반적인 문제", "md_examples_2tutorials_2README__KO.html#autotoc_md746", null ],
        [ "플랫폼별 고려사항", "md_examples_2tutorials_2README__KO.html#autotoc_md747", [
          [ "Windows", "md_examples_2tutorials_2README__KO.html#autotoc_md748", null ],
          [ "Linux", "md_examples_2tutorials_2README__KO.html#autotoc_md749", null ],
          [ "macOS", "md_examples_2tutorials_2README__KO.html#autotoc_md750", null ]
        ] ],
        [ "성능 최적화", "md_examples_2tutorials_2README__KO.html#autotoc_md751", null ],
        [ "네트워크 기능 테스트", "md_examples_2tutorials_2README__KO.html#autotoc_md752", [
          [ "로컬 테스트", "md_examples_2tutorials_2README__KO.html#autotoc_md753", null ],
          [ "네트워크 테스트", "md_examples_2tutorials_2README__KO.html#autotoc_md754", null ]
        ] ]
      ] ],
      [ "보안 고려사항", "md_examples_2tutorials_2README__KO.html#autotoc_md755", [
        [ "네트워크 보안", "md_examples_2tutorials_2README__KO.html#autotoc_md756", null ],
        [ "오류 처리", "md_examples_2tutorials_2README__KO.html#autotoc_md757", null ]
      ] ],
      [ "확장 포인트", "md_examples_2tutorials_2README__KO.html#autotoc_md758", [
        [ "새 샘플 추가", "md_examples_2tutorials_2README__KO.html#autotoc_md759", null ],
        [ "사용자 정의 프로토콜", "md_examples_2tutorials_2README__KO.html#autotoc_md760", null ],
        [ "통합 예제", "md_examples_2tutorials_2README__KO.html#autotoc_md761", null ]
      ] ],
      [ "도움말 얻기", "md_examples_2tutorials_2README__KO.html#autotoc_md762", null ],
      [ "라이선스", "md_examples_2tutorials_2README__KO.html#autotoc_md763", null ]
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
"classkcenon_1_1network_1_1connection__limiter.html#a04de6061d43f4656f35f2e974209a2d9",
"classkcenon_1_1network_1_1core_1_1messaging__client.html#a257b8fc6d5579e99ec1caf0bd549edad",
"classkcenon_1_1network_1_1core_1_1messaging__quic__client.html#a30b9d87b9d9e1116c241393f41d2651d",
"classkcenon_1_1network_1_1core_1_1messaging__quic__server.html#afd97b4b6f9d832e375501d01178144cd",
"classkcenon_1_1network_1_1core_1_1messaging__udp__client.html#a113521125f116cae07c9cadc6f111535",
"classkcenon_1_1network_1_1core_1_1messaging__udp__server.html#a7ac1de4e8a6cb37f01da556599b5c3f4",
"classkcenon_1_1network_1_1core_1_1messaging__ws__server.html#aa1b318a0bacabd6470e389196770cd17",
"classkcenon_1_1network_1_1core_1_1reliable__udp__client.html#a7a3ca92f4e1a393bf08a365a37d806f3",
"classkcenon_1_1network_1_1core_1_1secure__messaging__client.html#a4af0766982c499deb48e589bda321e84",
"classkcenon_1_1network_1_1core_1_1secure__messaging__server.html#a6322d54b30e459e7072e6574017937a9",
"classkcenon_1_1network_1_1core_1_1secure__messaging__udp__client.html#ab08dd5895cbe80077862c29c6c10a534",
"classkcenon_1_1network_1_1core_1_1secure__messaging__udp__server.html#af50d47e3ee4671a2c7b4464d8da3a555",
"classkcenon_1_1network_1_1core_1_1unified__messaging__client.html#a10144815286ac919798a4a1efad3dadd",
"classkcenon_1_1network_1_1core_1_1unified__session__manager.html#aa6c02d891c92c0569baf44fa02e21429",
"classkcenon_1_1network_1_1core_1_1unified__udp__messaging__server.html#aa54aa70147e24e4f5705d0bafc808adc",
"classkcenon_1_1network_1_1integration_1_1NetworkSystemBridge.html#af6e73a65f15ec149fb10ef49e2d87de4",
"classkcenon_1_1network_1_1integration_1_1basic__thread__pool.html#a54616e7051ac5fcd2494af58e9c86a4c",
"classkcenon_1_1network_1_1integration_1_1io__context__thread__manager_1_1impl.html#a656d1f8ae70c6e6750e92b079cde3aeb",
"classkcenon_1_1network_1_1integration_1_1thread__pool__interface.html#a36ea3fc34b4b3ac5380a08a3f71b83b7",
"classkcenon_1_1network_1_1interfaces_1_1i__quic__server.html#af9fe80950406eeff4b09ab4e771b9acd",
"classkcenon_1_1network_1_1internal_1_1adapters_1_1http__client__adapter.html#ada6d763479c067b4643d0914391ebaa7",
"classkcenon_1_1network_1_1internal_1_1adapters_1_1quic__server__adapter.html#ad011059e9207d40b59ee45ade3a97218",
"classkcenon_1_1network_1_1internal_1_1adapters_1_1ws__client__adapter.html#aa5c17b7b218a3bf4982d10c8e5759708",
"classkcenon_1_1network_1_1internal_1_1quic__socket.html#a1cd4ab8f2a7621989d6e02887a522dc5",
"classkcenon_1_1network_1_1internal_1_1tcp__socket.html#ada08f2f3adf27e723431132d8fdb23a4",
"classkcenon_1_1network_1_1metrics_1_1histogram.html#a3abb50a288f30a74e8e839e930397618",
"classkcenon_1_1network_1_1protocols_1_1grpc_1_1generic__service.html#ab1321ac16d8a508b64e675c0dbcb36f0",
"classkcenon_1_1network_1_1protocols_1_1grpc_1_1health__service.html#a5aae02f652b35e87e9f225ea5536f5dd",
"classkcenon_1_1network_1_1protocols_1_1http2_1_1goaway__frame.html#a05874ec45e67efcd393ff3c458ab84f1",
"classkcenon_1_1network_1_1protocols_1_1http2_1_1http2__server.html#a7adce9c7a684c31b494ad079ccdc57b2",
"classkcenon_1_1network_1_1protocols_1_1http2_1_1rst__stream__frame.html#aa929d74a3ae7f6cd2bfd1b42fe7b4156",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1connection.html#aa0a39cf16c43c4ea57f2d5696b5093dc",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1flow__controller.html#af2c39bbc5f9732adbb57f211adfca4db",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1pmtud__controller.html#a41204283118e8fc9fd544ddead2d61ab",
"classkcenon_1_1network_1_1protocols_1_1quic_1_1stream.html#a482b5f17e3c3ffb3e2c4ddceec7ead8e",
"classkcenon_1_1network_1_1rate__limiter.html#ac17ab634c88c433ad41a6658eb4ae6f9",
"classkcenon_1_1network_1_1tracing_1_1span.html#aef95632f79f44824b2ec829b3862c81f",
"classkcenon_1_1network_1_1unified_1_1adapters_1_1tcp__connection__adapter.html#a608b48a07df08dbed186e996d01588bc",
"classkcenon_1_1network_1_1unified_1_1adapters_1_1ws__connection__adapter.html#a4f56aca9a66a8d58df620b18d5fe25d0",
"classkcenon_1_1network_1_1utils_1_1callback__manager.html#a215fc3e40492b363a986a4bed9060e00",
"classkcenon_1_1network_1_1utils_1_1resilient__client.html#a99ab5f0b1fab1729a5527bff97320e26",
"config_2feature__flags_8h.html",
"frame__types_8h.html#a09df27ab1311da9794bcb50a0b9ffb62",
"http__facade_8cpp.html",
"md_examples_2README.html#autotoc_md647",
"module__kcenon_8network.html#a2d6bfe6ab66ca1ab6a1d28a35bbd63f5",
"modulemembers.html",
"namespacekcenon_1_1network_1_1internal.html#a86204ce59ef1afbff2a069cdfe73bae2a4cef2f30ac7d33419d00c1d93a090095",
"namespacekcenon_1_1network_1_1protocols_1_1http2.html#af0442b5506df78cb9075116c18a4a181a5e6fcb9c648d799cd888502c85db4b84",
"namespacekcenon_1_1network_1_1unified.html",
"quic__server__example_8cpp.html#a6e3e28a459cfc3150bf9971478239d40",
"src_2internal_2protocols_2http2_2frame_8h.html#af0442b5506df78cb9075116c18a4a181a4340fd73e75df7a9d9e45902a59ba3a4",
"structkcenon_1_1network_1_1core_1_1quic__client__config.html#a3bde44305c4b6b7f6d9744ebeb87b8e0",
"structkcenon_1_1network_1_1core_1_1unified__session__config.html#a8aa92a1891e177e99f49aff0b6888722",
"structkcenon_1_1network_1_1facade_1_1tcp__facade_1_1client__config.html#a8080649ff10ba31806fd858c392309a2",
"structkcenon_1_1network_1_1internal_1_1http__response.html#abdf3156dd27cfabad602aba4bca34f1e",
"structkcenon_1_1network_1_1protocol_1_1quic_1_1quic__config.html#a25a8a7c93655b54173beb4c142c4fb23",
"structkcenon_1_1network_1_1protocols_1_1http2_1_1http2__response.html",
"structkcenon_1_1network_1_1protocols_1_1quic_1_1max__streams__frame.html",
"structkcenon_1_1network_1_1protocols_1_1quic_1_1stream__frame.html#a984a13ff9d80881fa8d32be75ddd844f",
"structkcenon_1_1network_1_1utils_1_1connection__health.html#a254da75c2ea3a1aa5d305516d3bdbd75",
"udp__connection__adapter_8h.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';