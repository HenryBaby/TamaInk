# Licensing and Provenance Contract

Read this reference before importing, adapting, vendoring, generating from, or
distributing third-party code, text, data, images, fonts, waveform tables, test
vectors, binaries, or dependencies.

## Inspect before use

For every external source:

1. Read the repository license, notices, file-level headers, and contribution
   terms at the exact revision being considered.
2. Inspect vendored and generated material for transitive provenance.
3. Record project name, canonical URL, exact revision or version, relevant
   paths, copyright holders, SPDX license, required notices, and use type:
   dependency, unchanged copy, adapted copy, translated code, documentation,
   data, or inspiration-only reimplementation.
4. Confirm compatibility with TamaInk's repository license and intended binary
   distribution. Stop on ambiguity or missing licenses.
5. Prefer a pinned dependency over copying. Copy only when technically
   justified and preserve upstream headers plus a clear modification note.

Do not treat a public repository as permission to copy. Do not infer that the
top-level license covers every asset without checking notices and provenance.

## Maintain attribution with the change

- Update `THIRD_PARTY_NOTICES.md` in the same commit that introduces or changes
  third-party material.
- Preserve complete license texts where the upstream terms require them.
- Retain copyright and permission notices in substantial MIT-licensed copies.
- Mark adapted files with their upstream origin and TamaInk modifications.
- Attribute materially adapted documentation wording, even when a license would
  permit silent reuse.
- Keep generated output traceable to its source, generator, version, inputs, and
  license.
- Remove or update notices when third-party material is removed or replaced.

## Copyleft distribution

The repository currently declares AGPL-3.0. TamaLib and TamaTool declare
GPL-2.0-or-later in their source headers; FreeInk and CrossPoint declare MIT.
Before release, verify the actual pinned revisions and the compatibility of the
combined work rather than relying on these notes.

For distributed firmware containing copyleft code:

- provide the applicable license texts and warranty disclaimers;
- make complete corresponding source available for the exact binary, including
  build scripts, dependency revisions, interface definitions, and local
  modifications required by the license;
- state modifications and preserve upstream notices; and
- ensure recipients receive the required rights and source-access information.

Do not rely solely on a source repository link if the governing license or
distribution method requires more. Escalate uncertain interpretations for
qualified legal review before release.

## Release audit

Before publishing firmware:

1. Generate the resolved dependency/version inventory.
2. Compare it against `THIRD_PARTY_NOTICES.md` and packaged license files.
3. Search for copied headers, upstream names, embedded assets, generated tables,
   and binary blobs not present in the inventory.
4. Confirm that the published source exactly reconstructs the released binary.
5. Confirm no ROM, save, full-flash dump, proprietary firmware, or unlicensed
   asset is present.
6. Record the audit result and unresolved questions. Do not release with an
   unresolved license blocker.

