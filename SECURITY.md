# Security

RipStop is an obfuscation and integrity codec, not encryption. Assume a determined attacker can
recover data and embedded project values from a shipped executable.

Do not use RipStop to protect passwords, tokens, personal data, cryptographic keys, or any payload
requiring confidentiality.

Report security-sensitive defects privately through GitHub's security advisory workflow. Include
affected version, reproduction input, observed result, and expected result. Do not attach real
secrets or proprietary assets.
