# Combat Arena

An ordinary authored Kairo project. Move with WASD or the left stick to aim in the last nonzero movement direction. Press F to attack; each discrete press is consumed at a fixed physics tick, and presses during cooldown are discarded. The near crate needs two attacks. A solid wall protects the farther crate until the player moves around it. Destroyed crates lose their active physics bodies as well as their rendered visibility.

`Config/Combat.kcombat` uses `kairo-combat 1`, then `attack DAMAGE COOLDOWN_TICKS RANGE`, followed by `target ENTITY_ID HEALTH` records. Targets must be active static solid mesh roots tagged `kairo.damageable`, with no other incompatible gameplay/scene component roles. Raycasts ignore the player's own collider, skip trigger colliders, and stop at the first solid collider. Combat is horizontal, with no projectile travel, attack animation, enemy AI, or persisted target health.

Run `build/game-clang/Runtime/KairoPlayer/KairoPlayer Samples/CombatArena/Project.kproject` after building the game profile. `KairoPlayerCombatBridgeTests` checks actual raycasts, cooldown, occlusion and physics/render removal; portable tests verify the attack state independently.
