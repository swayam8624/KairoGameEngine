#include <cassert>

import Kairo.SceneECSBridge;
import Kairo.EngineCore;

int main()
{
    using namespace kairo::engine;
    using namespace kairo::runtime;

    Scene scene;
    const Entity root = scene.CreateEntity("Root");
    const Entity child = scene.CreateEntity("Child");
    scene.SetParent(child, root);
    scene.SetLayer(child, 7u);
    scene.AddTag(child, "player");
    scene.SetEnabled(root, false);

    SceneECSMirror runtime = SceneECSMirror::Extract(scene);
    assert(runtime.Size() == 2u);
    assert(runtime.Registry().EntityCount() == 2u);

    const auto runtimeRoot = runtime.RuntimeEntity(root);
    const auto runtimeChild = runtime.RuntimeEntity(child);
    assert(runtime.AuthoredEntity(runtimeRoot) == root);
    assert(runtime.AuthoredEntity(runtimeChild) == child);
    assert(runtime.Registry().Get<RuntimeName>(runtimeChild).Value == "Child");

    const RuntimeSceneState& childState =
        runtime.Registry().Get<RuntimeSceneState>(runtimeChild);
    assert(childState.Layer == 7u);
    assert(childState.Tags.size() == 1u && childState.Tags.front() == "player");
    assert(childState.Parent.has_value() && *childState.Parent == runtimeRoot);

    // Runtime data is extracted, not aliased. Authoring mutation is visible
    // only after the explicit refresh boundary is crossed.
    scene.Name(child).Value = "Renamed";
    assert(runtime.Registry().Get<RuntimeName>(runtimeChild).Value == "Child");
    runtime.RefreshAuthoredState(scene, child);
    assert(runtime.Registry().Get<RuntimeName>(runtimeChild).Value == "Renamed");

    return 0;
}
