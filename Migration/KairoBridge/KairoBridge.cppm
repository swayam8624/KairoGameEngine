// TEACHING MAP:
// Optional migration package module surface. Re-exports source project descriptors, canonical IR,
// manifests and Unity/Godot adapters. Read their tests to distinguish source parsing from complete
// gameplay conversion.
// See docs/SOURCE_READING_ORDER.md.

export module Kairo.Bridge;

export import Kairo.Bridge.Types;
export import Kairo.Bridge.SourceProject;
export import Kairo.Bridge.CanonicalIR;
export import Kairo.Bridge.Manifest;
export import Kairo.Bridge.UnityAdapter;
export import Kairo.Bridge.GodotAdapter;
