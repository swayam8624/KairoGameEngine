# Explicitly trusted native extension laboratory

Build with the integration workspace's BUILD_TESTING targets KairoPluginLab,
KairoExamplePlugin, KairoBadExamplePlugin and KairoPlayerPluginLoaderTests.
The regression loads actual native libraries and checks ABI rejection.

For a direct Linux run from the repository root (use a fresh temp directory):

    mkdir /tmp/kairo-plugin-lab
    c++ -std=c++23 -Wall -Wextra -Werror -shared -fPIC -IRuntime/KairoPlayer Samples/PluginLab/ExamplePlugin.cpp -o /tmp/kairo-plugin-lab/extension.so
    c++ -std=c++23 -Wall -Wextra -Werror -IRuntime/KairoPlayer Samples/PluginLab/PluginLab.cpp -ldl -o /tmp/kairo-plugin-lab/host
    printf 'kairo-plugin 1\nid double-value\nabi 1\nlibrary extension.so\n' > /tmp/kairo-plugin-lab/plugin.kplugin
    /tmp/kairo-plugin-lab/host --trusted-plugin /tmp/kairo-plugin-lab/plugin.kplugin 21

Expected: double-value: 42. Only load native libraries you trust. ABI
validation protects compatibility, not security: dynamic-library constructors
can execute before the query and native code has host privileges. The
manifest is not a signature. No automatic discovery, network download,
sandbox, editor callbacks, hot reload, or cross-version ABI guarantee beyond
the single v1 contract. Linux GCC and Clang have been tested; macOS and
Windows loaders are candidates pending platform CI.
