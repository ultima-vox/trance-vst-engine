# Orchestration

## Mode
Workflow mode with parent-session packet execution. Independent verification passes run sequentially in the parent session.

## Packets
| ID | Name | Type | Status |
|----|------|------|--------|
| A | Realtime Safety Audit | read-only | pending |
| B | Ownership & Lifetime Audit | read-only | pending |
| C | Timing Mode Verification | read-only | pending |
| D | Ratchet Verification | read-only | pending |
| E | Slide Verification | read-only | pending |
| F | RNG Audit | read-only | pending |
| G | Preset Semantics Audit | read-only | pending |
| H | Schema Versioning Audit | read-only | pending |
| I | Build & Test Verification | command | pending |
| J | Code Quality Audit | read-only | pending |

## Integration
After all packets complete, integrate findings into integration.md and produce final-report.md.
