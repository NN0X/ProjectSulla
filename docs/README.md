# Fixture documentation

Every circuit in `layouts/` is a real, documented part. This directory has one
subdirectory per layout, each containing:

- `README.md` - what the part is, its function/truth table, and how it is validated.
- a reference to the original datasheet / design document (linked, not redistributed,
  when the source is copyrighted).

All fixtures are validated for correctness in `make test`, compiled into `parts/` by
`make fixtures`, and benchmarked by `make perf`.
