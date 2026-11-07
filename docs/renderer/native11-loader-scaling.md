# Native loader geometry and API10

The native geometry integration candidate implements positive uniform loader
scaling before normal reconstruction. All 106 private runtime cases, 24 strict
host/sanitizer variants and 32 scoped Rust tests pass. This closes the bounded
loader-scaling gap below; stage 53 and full gameplay remain open.

## Correct geometry source

`ModelAssetInfo::parser_scalar_bits` retains the scalar passed to ReadDatabase.
The retained SOD bytes contain the original coordinates. API1–9 preserve their
original static parsing behavior. The new explicit API10 path scales original
positions, reconstructs normals, expands vertices, then uploads geometry.

The native node template already contains the loader's scaled translations.
Scaling the entire model or its final placement would scale them again. A final
part transform also cannot reproduce per-vertex binary32 rounding.

Draw-time arithmetic is separate from load-time arithmetic. The database bridge
records the entry control word and compares it with saved return state before
resetting the observer environment. A fifth private argument carries the encoded
load word only when these agree; otherwise it carries unknown. The model store
retains this separately from draw settings. The original loader still runs once
with its original arguments, return behavior and state.

## Pinned instruction evidence

Supported image SHA-256:
`4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de`.

In `6337a0–63384e`, the native scale routine uses ECX for the mesh and three
stack floats, returning with `ret 12`. It reads vertex count at `+c8`, output
positions at `+c0`, and retained original positions at `+fc`. When originals are
absent it allocates and copies them. Multiplication uses retained originals,
rather than repeatedly multiplying previously scaled output. Each component
uses x87 multiplication and a binary32 store. Virtual slot `+8` receives mask 7
afterward; downstream native normal/bounds invalidation remains separate proof.

## Implemented integration

API10 appends geometry-policy configuration and explicit native-world submission.
Its i686 table is 88 bytes, with appended pointers at 80 and 84. API1–9 layouts
and behavior remain unchanged. Native submission requires complete caster and
all-part state claims plus an exactly-once geometry policy callback. Unknown
arithmetic and nonpositive/nonfinite scales are refused.

The cold `SceneSubmission::Impl::load` / `load_model_content` path obtains policy
through the immutable bound snapshot before parsing, hashing and upload. Opaque
identifiers remain opaque. Policy callback or native/legacy scope changes reset
cached resources transactionally; source-epoch changes also trigger reload.
`WorldRenderSession::initialize_native_geometry` connects bound load metadata to
the actual API10 service, without substituting the observation's draw word.

The Rust adapter transforms each emitted mesh's owned original positions once,
validates the complete result, then reconstructs normals before any part sink.
Empty mesh nodes are skipped. Callback failure, bad output or malformed later
geometry cannot publish a partial model. Original file bytes are unchanged.

## Verification and limits

- Native position arithmetic matches in 24,576 comparisons covering 392,856
  vertices across all 12 supported x87 modes, including repeated scaling from
  retained originals. Native invalidation uses a synthetic callback in this
  isolated fixture; no game allocator, simulation or graphics is invoked.
- Loader bridge checks cover entry/return changes, argument order, stack balance,
  registers, flags, all occupied x87 slots and SSE state.
- GPU tests distinguish scaled/unscaled footprints while preserving node
  translation, check cache changes, callback validation, reentry and failure
  rollback, and exercise the owned-store → bound-frame → API10 path.
- Retained frames survive source-store clear. Unknown load arithmetic fails
  despite known draw arithmetic without accepting a new frame.
- Native normal/invalidation parity, runtime material overrides, complete-world
  production and independent startup/presentation remain separate requirements.

The [checkpoint receipt](../../build/native11/native-geometry-milestone.json)
records source and artifact hashes. No installed game files changed.
