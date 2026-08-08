from pathlib import Path

p=Path('Runtime/KairoPlayer/CMakeLists.txt')
s=p.read_text().replace('RuntimeInputBridge.cppm RuntimeLogicBridge.cppm RuntimeNativeGameplayBridge.cppm RuntimePackaging.cppm)', 'RuntimeInputBridge.cppm RuntimeLogicBridge.cppm RuntimeNativeGameplayBridge.cppm RuntimeProductionSystemsBridge.cppm RuntimePackaging.cppm)')
s=s.replace('add_executable(KairoPlayerNativeGameplayTests tests/RuntimeNativeGameplayTests.cpp)\n    target_link_libraries(KairoPlayerNativeGameplayTests PRIVATE KairoPlayerRuntime)\n    add_test(NAME KairoPlayerNativeGameplayTests COMMAND KairoPlayerNativeGameplayTests)', 'add_executable(KairoPlayerNativeGameplayTests tests/RuntimeNativeGameplayTests.cpp)\n    target_link_libraries(KairoPlayerNativeGameplayTests PRIVATE KairoPlayerRuntime)\n    add_test(NAME KairoPlayerNativeGameplayTests COMMAND KairoPlayerNativeGameplayTests)\n\n    add_executable(KairoPlayerProductionSystemsTests tests/RuntimeProductionSystemsTests.cpp)\n    target_link_libraries(KairoPlayerProductionSystemsTests PRIVATE KairoPlayerRuntime)\n    add_test(NAME KairoPlayerProductionSystemsTests COMMAND KairoPlayerProductionSystemsTests)')
p.write_text(s)

p=Path('Runtime/KairoPlayer/main.cpp')
s=p.read_text().replace('import Kairo.Player.RuntimeNativeGameplayBridge;\n', 'import Kairo.Player.RuntimeNativeGameplayBridge;\nimport Kairo.Player.RuntimeProductionSystemsBridge;\n')
s=s.replace('kairo::player::RuntimeNativeGameplayBridge nativeGameplay(\n            project, kairo::player::PlayerNativeGameplayRegistry());', 'kairo::player::RuntimeNativeGameplayBridge nativeGameplay(\n            project, kairo::player::PlayerNativeGameplayRegistry());\n        kairo::player::RuntimeProductionSystemsBridge production(project);')
s=s.replace('std::cout << "  native behaviours: " << nativeGameplay.InstanceCount() << \'\\n\';', 'std::cout << "  native behaviours: " << nativeGameplay.InstanceCount() << \'\\n\'\n                  << "  production systems: " << (production.Enabled() ? "enabled" : "disabled") << \'\\n\';')
s=s.replace('nativeGameplay.Update(static_cast<double>(elapsedSeconds));\n            renderer.SubmitRenderScene', 'nativeGameplay.Update(static_cast<double>(elapsedSeconds));\n            production.Step(static_cast<double>(elapsedSeconds));\n            renderer.SubmitRenderScene')
p.write_text(s)

Path('.github/workflows/wire-production-player.yml').unlink()
Path('.github/scripts/wire_production_player.py').unlink()
