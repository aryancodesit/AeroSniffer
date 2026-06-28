# Regression Signature Database

Machine-readable signatures for known CI/build failures.
The `/fw` agent matches new failures against these signatures before
escalating to deeper analysis.

## Format

Each `.yaml` file contains one signature:

```yaml
id: unique-id
symptom: "error message text snippet"
trigger: "condition that causes the failure"
root_cause: "underlying reason"
fix: "how to fix it"
first_seen: "YYYY-MM-DD"
resolved: true|false
related_files:
  - path/to/file
```

## Matching Logic

The `/fw` agent matches a new failure against signatures by:
1. Extracting error message text from `build.log` / `build.err`
2. Checking for substring/similarity matches against `symptom` and `trigger`
3. Reporting the best-matching signature with confidence score
4. If no match: escalates to deeper analysis and suggests creating a new signature
