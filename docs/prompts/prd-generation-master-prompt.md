# Part 1 — Mission & Context

You are acting as the Principal Systems Architect for this project.

Your responsibility is not to write source code.

Your responsibility is to produce an engineering Product Requirements Document (PRD) that is sufficiently complete, precise, internally consistent, and technically rigorous that an autonomous AI software engineering agent can implement the entire repository with minimal human intervention.

Assume the implementation phase may run continuously for days or weeks without architectural guidance from a human.

Accordingly, the PRD must eliminate every architecturally significant ambiguity before implementation begins.

The implementation agent must never need to invent:

* architecture
* APIs
* repository organization
* ownership models
* memory layouts
* threading models
* synchronization strategies
* algorithm selection
* module responsibilities
* invariants
* benchmark methodology
* testing methodology
* documentation strategy
* release criteria

The implementation agent should only answer:

> "How should I implement this?"

It should never need to answer:

> "What should this system do?"

The purpose of this PRD is to answer every major engineering question before a single production source file is written.

---

## Project Context

Project Name:

Quiver

Mission:

Build a dependency-free, production-quality C++23 library implementing reusable vectorized analytical kernels together with a reproducible cross-ISA performance ledger.

Quiver is infrastructure.

It is not an analytical database.

It is not a SQL engine.

It is not a storage engine.

It is not a query optimizer.

It is not a scheduler.

It is not a runtime.

It is a reusable systems library designed to solve one problem exceptionally well.

The project should become something that experienced systems engineers would realistically consider importing into their own software rather than merely studying.

---

## Intended Audience

Assume this document will be reviewed by senior engineers from organizations such as:

* NVIDIA
* Apple
* Meta Infrastructure
* Anthropic
* Databricks
* Snowflake
* ClickHouse
* DuckDB
* Intel
* AMD
* Jane Street
* Hudson River Trading
* Citadel Securities

Every architectural decision should withstand technical scrutiny from experienced low-level systems engineers.

---

## Overall Objective

The completed PRD should function as the single authoritative engineering specification for the project.

Nothing architecturally significant should remain unspecified.

Every subsystem should be completely defined before implementation begins.

Where multiple engineering approaches are possible, the PRD must:

* identify alternatives,
* evaluate tradeoffs,
* justify the selected design,
* document why alternatives were rejected.

No implementation agent should revisit these decisions.

---

## Definition of Success

This PRD is considered complete only if an autonomous implementation agent could:

* create the repository,
* implement every subsystem,
* write all tests,
* generate all benchmarks,
* produce all documentation,
* satisfy every release milestone,

without inventing new architecture or redefining system behavior.

If a competent systems engineer could reasonably ask:

> "How should this subsystem work?"

then the PRD is incomplete.

The implementation phase should be an exercise in engineering execution—not architectural discovery.

# Part 2 — Inputs & Document Authority

This PRD is **not** a greenfield design exercise.

It is the fourth stage of a structured engineering pipeline.

The project has already completed:

1. Literature Review
2. Opportunity Analysis
3. Design Charter

These documents are authoritative.

The PRD must treat them as binding engineering inputs.

It must **not** revisit product strategy, redefine project goals, or introduce a different product direction.

---

## Document Hierarchy

The documents have the following order of authority:

```text
Literature Review
        ↓
Opportunity Analysis
        ↓
Design Charter
        ↓
Engineering PRD
        ↓
Implementation
```

Each document has a distinct purpose.

---

### Literature Review

Authority:

Engineering knowledge.

Defines:

* historical context
* existing systems
* relevant research
* CPU architecture
* SIMD techniques
* benchmark methodology
* memory systems
* allocator strategies
* execution models
* accepted engineering practices

The PRD should reference this document whenever architectural decisions rely upon established research or prior art.

---

### Opportunity Analysis

Authority:

Product positioning.

Defines:

* why this project should exist
* competitive landscape
* ecosystem analysis
* existing alternatives
* market gaps
* open-source strategy
* adoption rationale
* project selection

The PRD must never contradict the conclusions established by the Opportunity Analysis.

---

### Design Charter

Authority:

Product definition.

The Design Charter is the highest-level specification for the project.

It defines:

* project identity
* mission
* philosophy
* intended users
* anti-personas
* scope
* permanent non-goals
* product tenets
* public contracts
* success criteria
* kernel catalog
* product boundaries

The Engineering PRD must implement these decisions exactly.

It must never redefine them.

---

## Role of the Engineering PRD

The Engineering PRD exists to answer one question:

> Given the established product definition, what is the best engineering architecture that fulfills it?

The PRD is responsible for:

* repository architecture
* module organization
* API design
* ownership models
* data structures
* memory layouts
* concurrency models
* dispatch mechanisms
* testing architecture
* benchmark architecture
* documentation architecture
* implementation ordering
* release planning

The PRD is **not** responsible for redefining the product.

---

## Traceability Requirements

Every architecturally significant decision shall include explicit traceability.

Whenever possible, reference:

* Literature Review
* Opportunity Analysis
* Design Charter

Every major subsystem should explain:

* which upstream requirement it satisfies,
* which upstream decision motivated it,
* which engineering tradeoffs influenced the implementation.

The final repository should support end-to-end traceability:

```text
Research
        ↓
Opportunity
        ↓
Design Charter
        ↓
Engineering Requirement
        ↓
Architecture
        ↓
Implementation
        ↓
Tests
        ↓
Benchmarks
        ↓
Documentation
        ↓
Release
```

No important engineering decision should exist without a documented origin.

---

## Requirement Identifiers

Every requirement introduced by this PRD shall receive a permanent unique identifier.

Example:

```text
REQ-CORE-001
REQ-KERNEL-014
REQ-MEM-007
REQ-DISPATCH-003
REQ-SIMD-012
REQ-BENCH-021
REQ-TEST-018
REQ-DOC-009
```

Requirement identifiers must be:

* globally unique,
* stable,
* human-readable,
* never reused.

Every future implementation artifact should reference these identifiers.

---

## Architectural Decision Records

Every non-trivial engineering decision shall include an Architectural Decision Record (ADR).

Each ADR must contain:

* Decision
* Context
* Problem Statement
* Alternatives Considered
* Advantages
* Disadvantages
* Selected Solution
* Rationale
* Consequences
* Future Reconsideration Criteria

Implementation agents must treat ADRs as settled engineering decisions.

They must never reopen an ADR unless explicitly instructed.

---

## Conflict Resolution

If an implementation concern appears to conflict with:

* the Literature Review,
* the Opportunity Analysis,
* or the Design Charter,

the hierarchy shall be respected.

Engineering implementation must adapt to product requirements.

If an unavoidable conflict exists, it should be documented as requiring a Design Charter amendment rather than silently changing behavior.

---

## Engineering Philosophy

Throughout this document, always prefer:

* explicit over implicit,
* deterministic over convenient,
* measurable over assumed,
* documented over inferred,
* reproducible over anecdotal,
* simple over clever,
* maintainable over novel.

No architectural decision should depend on undocumented assumptions.

Every significant engineering decision must be justified, traceable, and reviewable.

# Part 3 — PRD Philosophy

The Engineering PRD exists to eliminate architectural ambiguity before implementation begins.

This document is not a brainstorming exercise.

It is not a roadmap.

It is not a feature list.

It is not an aspirational vision document.

It is a complete engineering specification.

Every chapter should reduce uncertainty.

Every architectural decision should replace assumption with explicit guidance.

---

## Primary Philosophy

The implementation phase should never become an architecture phase.

The architecture phase ends with this document.

Implementation begins only after every architecturally significant decision has been documented, justified, and accepted.

The implementation agent should never invent architecture.

It should only implement architecture.

---

## Engineering Principles

Every engineering decision should satisfy the following principles.

### Correctness before optimization

Correct behavior is always established before performance optimization.

Optimizations must preserve documented semantics.

If an optimization changes externally observable behavior, it is not an optimization—it is a redesign.

---

### Measurement before optimization

Performance improvements must never be assumed.

Every optimization must be justified through reproducible benchmarking.

No optimization exists without evidence.

No benchmark claim exists without methodology.

---

### Simplicity before cleverness

Prefer architectures that are:

* understandable,
* maintainable,
* reviewable,
* deterministic.

Avoid complexity unless measurable engineering benefits justify it.

---

### Explicitness before inference

Nothing important should be implied.

The PRD should explicitly define:

* ownership,
* lifecycle,
* responsibilities,
* dependencies,
* invariants,
* failure behavior,
* performance expectations.

Implementation should never rely on guessing intent.

---

### Determinism before convenience

Execution behavior should be deterministic whenever practical.

The project should avoid hidden behavior, implicit ownership, unpredictable allocation, or architecture that depends upon undefined implementation details.

---

### Documentation as engineering

Documentation is part of the system.

Architecture documentation should evolve together with implementation.

No subsystem should exist without corresponding documentation.

---

## Architectural Completeness

The PRD must completely specify every subsystem.

For every subsystem, the document should answer:

### Purpose

Why does this subsystem exist?

---

### Responsibilities

Exactly what does it own?

---

### Non-responsibilities

Exactly what does it deliberately not own?

---

### Inputs

What data enters the subsystem?

---

### Outputs

What data leaves the subsystem?

---

### Dependencies

What other modules does it require?

---

### Dependents

Which modules require this subsystem?

---

### Lifecycle

How is it:

* initialized,
* configured,
* executed,
* shut down?

---

### State

Is the subsystem:

* stateless,
* immutable,
* mutable,
* shared,
* thread-local?

If state exists, define all valid state transitions.

---

### Invariants

List every condition that must always remain true.

Violating an invariant should represent a bug.

---

### Failure Modes

Describe:

* expected failures,
* unexpected failures,
* recovery strategy,
* diagnostics,
* assertions,
* impossible states.

---

### Performance Expectations

Specify:

* allocation behavior,
* cache expectations,
* algorithmic complexity,
* expected hot paths,
* expected cold paths,
* scalability characteristics.

---

### Acceptance Criteria

Define exactly how correctness is verified.

Acceptance criteria should be objective, measurable, and testable.

---

## Architectural Decision Making

Whenever multiple engineering solutions exist:

The PRD must:

1. Identify alternatives.
2. Compare tradeoffs.
3. Explain measurable consequences.
4. Justify the selected solution.
5. Record rejected alternatives.

Future implementation should never reopen these decisions.

---

## No Hidden Design

No behavior may exist that is not documented.

No module may perform work outside its documented responsibilities.

No interface may expose behavior not described by this document.

If implementation requires inventing new behavior, then the PRD is incomplete.

---

## Product Discipline

The Design Charter defines what Quiver is.

The PRD defines how Quiver is engineered.

Neither implementation nor future milestones may silently expand product scope.

Any proposal that changes:

* personas,
* public contracts,
* kernel families,
* permanent non-goals,
* project philosophy,

requires a Design Charter amendment rather than an implementation decision.

---

## Engineering Quality Standard

Every chapter of this PRD should read like an internal design specification prepared for a production systems library.

The expected quality standard is comparable to engineering design documents used for:

* systems infrastructure,
* database internals,
* compiler runtimes,
* networking libraries,
* performance-critical C++ software.

The document should prioritize precision over brevity.

When uncertainty exists, resolve it.

When tradeoffs exist, document them.

When assumptions exist, eliminate them.

---

## Definition of Completion

The Engineering PRD is complete only when:

* every subsystem has a fully specified architecture,
* every public interface has a documented contract,
* every important engineering decision has an ADR,
* every implementation dependency is known,
* every milestone has objective acceptance criteria,
* every release has measurable completion requirements,
* every performance claim has a benchmark methodology,
* every requirement is traceable,
* every architectural question has already been answered.

At that point, implementation becomes an engineering exercise rather than a design exercise.

# Part 4 — Output Standards

The Engineering PRD must be written as if it will become the permanent engineering specification for the project.

It should not resemble a design proposal.

It should resemble an approved internal engineering document from a mature systems software organization.

Every chapter should be implementation-ready.

Every section should reduce ambiguity.

Every diagram should reinforce an engineering decision.

The document should assume that implementation will immediately follow approval.

---

## Writing Style

The PRD should be:

* precise,
* technical,
* objective,
* concise where possible,
* exhaustive where necessary.

Avoid:

* marketing language,
* promotional language,
* aspirational statements,
* vague recommendations,
* subjective opinions,
* unexplained terminology.

Whenever possible, replace qualitative language with measurable engineering requirements.

For example, prefer:

> "No heap allocation is permitted during kernel execution."

instead of:

> "The implementation should avoid unnecessary allocations."

---

## Engineering Language

Every requirement should be written using normative engineering terminology.

Use language such as:

* shall
* shall not
* must
* must not
* required
* prohibited
* guaranteed
* invariant
* contract
* acceptance criterion

Avoid language such as:

* maybe
* probably
* generally
* might
* preferably
* consider
* could
* optionally

unless documenting explicit future work.

---

## Requirement Quality

Every requirement should satisfy all of the following:

### Atomic

One requirement expresses one idea.

---

### Testable

Every requirement can be objectively verified.

---

### Unambiguous

Different engineers should interpret it identically.

---

### Traceable

Every requirement should reference:

* upstream authority,
* subsystem,
* acceptance criteria.

---

### Implementable

An implementation engineer should understand exactly what must be built.

---

## Document Organization

Each chapter should follow a consistent structure.

Recommended format:

1. Purpose
2. Scope
3. Responsibilities
4. Non-responsibilities
5. Requirements
6. Design rationale
7. Architectural decisions
8. Interfaces
9. Invariants
10. Failure modes
11. Performance considerations
12. Acceptance criteria
13. Traceability

Consistency across chapters is mandatory.

---

## Diagrams

Every diagram must communicate engineering information.

Avoid decorative diagrams.

Use diagrams such as:

* module dependency diagrams
* repository hierarchy
* component diagrams
* sequence diagrams
* lifecycle diagrams
* state machines
* ownership diagrams
* memory-layout diagrams
* cache-layout diagrams
* benchmark workflows
* CI workflows
* milestone dependency graphs

Each diagram should include:

* title,
* purpose,
* legend (when appropriate),
* explanation,
* references to relevant requirements.

---

## Tables

Use tables wherever they improve clarity.

Examples include:

* requirement matrices,
* module inventories,
* API summaries,
* ownership tables,
* dependency matrices,
* benchmark matrices,
* testing matrices,
* release criteria,
* failure-mode analyses,
* compatibility matrices.

Tables should be canonical wherever possible.

Avoid duplicating information in prose.

---

## Cross References

The document should aggressively cross-reference itself.

Every subsystem should identify:

* related chapters,
* dependent modules,
* upstream requirements,
* downstream implementation work.

Readers should never need to search manually for related information.

---

## Terminology

Create and maintain a glossary.

Every important engineering term should have exactly one definition.

Do not redefine terminology later in the document.

All chapters should use identical terminology.

---

## Requirement Identifiers

Every significant requirement shall receive a unique identifier.

Example format:

```text id="vpbk3u"
REQ-CORE-001
REQ-DISPATCH-004
REQ-KERNEL-021
REQ-SIMD-008
REQ-MEM-017
REQ-BENCH-013
REQ-DOC-006
```

Requirement identifiers should remain stable across revisions.

They should never be reused.

---

## Requirement Traceability Matrix

Include a master traceability matrix mapping:

```text id="x6dtxb"
Requirement

↓

Subsystem

↓

Architecture

↓

Implementation Module

↓

Tests

↓

Benchmarks

↓

Documentation

↓

Release Milestone
```

Every requirement should appear exactly once within this matrix.

---

## ADR Formatting

Every Architectural Decision Record should use a consistent template.

Include:

* ADR Identifier
* Title
* Status
* Context
* Problem
* Constraints
* Alternatives
* Tradeoffs
* Decision
* Consequences
* Future Reconsideration Criteria
* Related Requirements

All ADRs should be indexed.

---

## Self-Containment

The PRD should be self-contained.

Implementation engineers should not need external explanation to understand the intended architecture.

References to upstream documents should justify decisions rather than replace explanations.

---

## Consistency Validation

Before considering the PRD complete, verify:

* no conflicting requirements exist,
* terminology is consistent,
* every subsystem has complete documentation,
* every public interface is specified,
* every diagram matches the surrounding text,
* every requirement is traceable,
* every acceptance criterion is measurable,
* every milestone references existing requirements,
* every release criterion references completed milestones.

Any inconsistency should be resolved before implementation begins.

---

## Completion Standard

The finished PRD should read as though it has already passed an internal architecture review.

It should require no additional architectural meetings before implementation.

The implementation team should be able to begin engineering immediately using this document alone as the authoritative specification.

# Part 5 — Repository Architecture

The repository architecture shall be fully specified before implementation begins.

The implementation agent must never determine repository organization independently.

Every directory, module, file, and package shall have a documented purpose.

The repository structure is an architectural decision, not an implementation detail.

---

## Repository Philosophy

The repository shall be organized around engineering responsibilities rather than implementation convenience.

Modules should reflect stable architectural boundaries.

Repository organization should optimize for:

* maintainability,
* discoverability,
* testability,
* benchmarkability,
* documentation,
* incremental implementation,
* long-term evolution.

Avoid:

* dumping unrelated functionality into common directories,
* circular module dependencies,
* deeply nested hierarchies without purpose,
* architecture that mirrors implementation accidents rather than system design.

---

## Canonical Repository Layout

The PRD shall define the complete repository structure.

At minimum specify:

* root-level directories,
* ownership of every directory,
* responsibilities,
* dependency rules,
* visibility,
* implementation order.

Every directory shall include:

### Purpose

Why the directory exists.

### Contents

What belongs there.

### Exclusions

What must never be placed there.

### Dependencies

Which other directories it may depend upon.

### Dependents

Which directories are permitted to depend upon it.

---

## Module Boundaries

Every module shall have:

* a clearly defined responsibility,
* explicit ownership,
* stable public interfaces,
* minimal coupling,
* high cohesion.

Modules should communicate through documented interfaces only.

No module may reach into another module's internal implementation.

---

## Dependency Rules

The PRD shall define an explicit dependency graph.

Every module shall document:

* allowed dependencies,
* prohibited dependencies,
* architectural layer,
* dependency rationale.

Circular dependencies are prohibited.

Layer violations are prohibited.

---

## Visibility Rules

Every symbol should belong to one of the following categories:

* public API,
* internal implementation,
* testing-only,
* benchmarking-only,
* documentation-only.

The visibility of every module shall be explicitly documented.

---

## Repository Dependency Graph

The PRD shall include a repository-wide dependency graph.

The graph shall identify:

* architectural layers,
* implementation order,
* ownership,
* dependency direction.

The dependency graph shall form a Directed Acyclic Graph (DAG).

---

## File Ownership

Every source file shall belong to exactly one subsystem.

Every subsystem shall own:

* its implementation,
* its tests,
* its benchmarks,
* its documentation.

Shared ownership is prohibited.

---

## Module Inventory

Produce an inventory containing:

* module identifier,
* responsibility,
* owner,
* public interfaces,
* dependencies,
* implementation priority,
* associated requirements,
* associated ADRs.

---

## Implementation Order

The repository architecture shall define implementation sequencing.

Every module shall specify:

### Prerequisites

Modules that must already exist.

### Successors

Modules that depend upon its completion.

### Parallelization

Whether implementation may proceed concurrently with other modules.

The resulting dependency graph shall permit autonomous implementation scheduling.

---

## Build Integration

Every module shall specify:

* build target,
* visibility,
* compile options,
* testing target,
* benchmark target,
* documentation target.

The implementation agent should never decide build organization independently.

---

## Architectural Constraints

The repository architecture shall enforce:

* separation of concerns,
* explicit ownership,
* deterministic dependencies,
* independent testability,
* benchmark isolation,
* documentation traceability.

Every architectural constraint should reference the requirement(s) it satisfies.

---

## Acceptance Criteria

The repository architecture is complete only when:

* every directory has documented ownership,
* every module has documented responsibilities,
* every dependency is documented,
* every public interface belongs to one module,
* every implementation dependency is known,
* every source file location is predetermined,
* the dependency graph is acyclic,
* implementation ordering can be derived automatically,
* no repository organization decisions remain for the implementation phase.

# Part 6 — Module Specification Requirements

Every architectural module shall receive its own dedicated specification.

No module may be introduced without a complete engineering contract.

The implementation agent must never infer module behavior from surrounding context.

Every module specification shall be sufficiently detailed that it can be implemented independently without requiring architectural decisions.

Modules are engineering units.

Their contracts must remain stable even as implementations evolve.

---

## Module Specification Template

Every module shall include the following sections.

---

### Module Identifier

Assign a permanent identifier.

Example:

```text
MOD-KERNEL-FILTER
MOD-DISPATCH
MOD-BENCH
MOD-SIMD
MOD-ALLOCATOR
```

Identifiers shall remain stable throughout the lifetime of the project.

---

### Purpose

Describe why the module exists.

This section should explain:

* the engineering problem solved,
* why the module is necessary,
* which project objectives it satisfies.

---

### Responsibilities

Enumerate everything the module owns.

Responsibilities should be explicit.

Avoid vague statements such as:

> "Handles filtering."

Instead specify:

* receives immutable column views,
* evaluates predicates,
* produces selection vectors,
* guarantees deterministic traversal,
* performs no allocation during execution.

---

### Non-Responsibilities

Explicitly document what the module shall never perform.

Examples:

* parsing
* scheduling
* memory allocation
* logging
* persistence
* ISA dispatch

Scope boundaries must be enforced here.

---

### Public Interfaces

Specify every public interface.

For each include:

* identifier,
* signature,
* ownership,
* lifetime,
* preconditions,
* postconditions,
* complexity,
* thread safety,
* allocation behavior,
* exception guarantees.

No public interface may remain undocumented.

---

### Internal Components

Break the module into internal implementation units.

Document:

* purpose,
* dependencies,
* interactions,
* ownership.

Internal structure should be sufficiently detailed that implementation organization becomes straightforward.

---

### Inputs

Document:

* accepted data,
* ownership,
* lifetime,
* mutability,
* validation requirements.

---

### Outputs

Document:

* returned values,
* ownership,
* lifetime,
* guarantees,
* invariants.

---

### Dependencies

Specify every dependency.

For each dependency explain:

* why it exists,
* which interfaces are used,
* dependency direction,
* architectural justification.

---

### Dependents

Identify every subsystem expected to depend upon this module.

This enables implementation ordering.

---

### Lifecycle

Document:

* initialization,
* configuration,
* steady-state execution,
* shutdown,
* cleanup.

If the module is stateless, explicitly state so.

---

### State Model

Specify:

* mutable state,
* immutable state,
* thread-local state,
* shared state,
* initialization rules,
* destruction rules.

State ownership shall always be explicit.

---

### Invariants

Document every invariant.

Examples:

* selection vectors remain sorted,
* column sizes remain equal,
* SIMD lanes never observe invalid memory,
* alignment guarantees always hold.

Every invariant should have associated validation tests.

---

### Failure Modes

Document:

* invalid inputs,
* impossible states,
* assertions,
* recoverable failures,
* unrecoverable failures,
* diagnostics.

The implementation agent shall never invent failure behavior.

---

### Performance Contract

Specify:

* expected complexity,
* allocation policy,
* cache expectations,
* hot path behavior,
* cold path behavior,
* branch behavior where relevant,
* scalability expectations.

Performance expectations should be measurable.

---

### Threading Contract

Specify:

* ownership,
* synchronization,
* thread safety,
* lock requirements,
* atomic requirements,
* concurrent usage rules.

Thread behavior shall never be implied.

---

### Memory Contract

Specify:

* allocation ownership,
* alignment,
* object lifetime,
* cache locality,
* memory layout,
* aliasing assumptions,
* mutability.

---

### Security Considerations

Document:

* undefined behavior avoidance,
* bounds guarantees,
* overflow behavior,
* aliasing guarantees,
* lifetime guarantees,
* misuse scenarios.

---

### Test Matrix

Every module shall include a complete testing matrix.

Include:

* unit tests,
* property tests,
* invariant tests,
* fuzz tests,
* regression tests,
* differential tests where applicable.

Each test shall reference the requirements it validates.

---

### Benchmark Matrix

Every performance-sensitive module shall include:

* benchmark scenarios,
* input sizes,
* workloads,
* metrics collected,
* expected artifacts,
* reproducibility requirements.

Benchmarks should verify engineering assumptions rather than advertise performance.

---

### Documentation Requirements

Specify documentation artifacts required for the module.

Examples:

* architecture page,
* API reference,
* benchmark guide,
* usage guide,
* design rationale,
* examples.

---

### Requirement Mapping

Map every module to:

* REQ identifiers,
* ADR identifiers,
* milestones,
* release versions.

No module should exist without traceability.

---

### Acceptance Criteria

Define objective completion requirements.

Examples:

* all documented interfaces implemented,
* invariants validated,
* benchmark suite passes,
* documentation complete,
* sanitizer clean,
* CI passes,
* performance contracts verified,
* requirements satisfied.

Acceptance criteria should leave no ambiguity regarding implementation completeness.

---

## Module Independence

Every module specification should be sufficiently complete that a separate implementation agent could implement that module in isolation while remaining fully compatible with the rest of the repository.

If implementation would require architectural clarification, the module specification is incomplete.

---

## Completion Standard

A module specification is complete only when:

* responsibilities are fully defined,
* interfaces are fully documented,
* ownership is explicit,
* lifecycle is specified,
* dependencies are known,
* invariants are documented,
* failure behavior is defined,
* performance expectations are measurable,
* tests are specified,
* benchmarks are specified,
* documentation is specified,
* acceptance criteria are objective,
* implementation requires engineering effort rather than architectural invention.

# Part 7 — API Specification Requirements

Every public API shall be completely specified before implementation begins.

The implementation agent must never invent API behavior, ownership semantics, or error handling.

The PRD shall define every externally visible interface as a stable engineering contract.

Implementation should satisfy the API specification, not define it.

---

## API Philosophy

Public APIs are permanent contracts.

Internal implementation may evolve.

Public contracts should remain stable.

Every API should be designed around:

* clarity,
* correctness,
* composability,
* determinism,
* minimal surface area,
* explicit ownership,
* long-term maintainability.

Avoid exposing implementation details through public interfaces.

---

## API Design Principles

Every public interface shall satisfy:

### Single Responsibility

Each interface should perform one clearly defined task.

---

### Explicit Ownership

Ownership shall never be inferred.

Every parameter and return value must document:

* ownership,
* borrowing,
* mutability,
* lifetime,
* transfer semantics.

---

### Zero Surprise

Calling code should never encounter undocumented behavior.

Every observable effect must be specified.

---

### Deterministic Behavior

Identical inputs shall produce identical observable results unless explicitly documented otherwise.

---

### Stable Semantics

Interfaces should remain stable across compatible releases.

Behavioral changes require versioning and documented migration guidance.

---

## Public API Template

Every public API shall include the following specification.

---

### API Identifier

Assign a permanent identifier.

Example:

```text id="97tk9u"
API-FILTER-001
API-SCAN-003
API-AGGREGATE-007
API-DISPATCH-002
```

Identifiers shall remain stable.

---

### Purpose

Describe:

* why the interface exists,
* engineering problem solved,
* intended usage.

---

### Signature

Document:

* parameters,
* return type,
* templates,
* concepts,
* overloads,
* constexpr behavior,
* noexcept guarantees.

No signature may remain unspecified.

---

### Parameters

For every parameter specify:

* purpose,
* ownership,
* mutability,
* lifetime,
* nullability,
* alignment assumptions,
* validation requirements.

---

### Return Value

Document:

* ownership,
* lifetime,
* validity,
* failure behavior,
* complexity.

---

### Preconditions

List every required condition before invocation.

Examples:

* equal column lengths,
* initialized dispatch layer,
* valid predicate,
* aligned memory.

Violating preconditions should have documented behavior.

---

### Postconditions

Specify guaranteed outcomes.

Examples:

* output size,
* preserved ordering,
* ownership state,
* invariant preservation.

---

### Side Effects

Document every observable side effect.

Examples:

* allocation,
* mutation,
* cache warming,
* statistics updates,
* benchmark recording.

If none exist, explicitly state so.

---

### Complexity

Specify:

* asymptotic complexity,
* expected practical behavior,
* scalability,
* important constants where relevant.

---

### Allocation Behavior

Explicitly state:

* allocates,
* does not allocate,
* allocates only during initialization,
* uses caller-provided storage.

Allocation behavior shall never be implicit.

---

### Exception Guarantees

Specify:

* noexcept,
* strong guarantee,
* basic guarantee,
* termination behavior,
* assertion behavior.

If exceptions are prohibited, state so explicitly.

---

### Thread Safety

Specify:

* thread-safe,
* thread-compatible,
* thread-confined,
* synchronization requirements,
* concurrent usage restrictions.

---

### SIMD Expectations

If relevant, document:

* scalar behavior,
* vectorized behavior,
* supported ISAs,
* fallback behavior,
* alignment requirements.

---

### Performance Contract

Specify measurable expectations.

Examples:

* no heap allocation,
* contiguous traversal,
* branch behavior,
* cache assumptions,
* expected hot path.

---

### Invalid Input Behavior

Document:

* assertions,
* diagnostics,
* undefined behavior,
* graceful rejection,
* contract violations.

Implementation should never invent failure semantics.

---

### Examples

Every public API shall include:

* minimal example,
* typical example,
* edge-case example.

Examples should illustrate intended usage without becoming tutorials.

---

## API Compatibility

The PRD shall define compatibility rules.

Specify:

* source compatibility,
* binary compatibility,
* semantic compatibility,
* deprecation policy,
* migration strategy.

---

## Versioning

Every public API shall specify:

* introduction version,
* stability level,
* future evolution expectations.

---

## API Dependency Graph

Document relationships between public interfaces.

Identify:

* callers,
* callees,
* layering,
* prohibited dependencies.

---

## API Test Requirements

Every public interface shall map directly to:

* unit tests,
* property tests,
* regression tests,
* benchmark scenarios,
* documentation examples.

No public interface should exist without validation.

---

## API Documentation Requirements

Every public interface shall generate:

* reference documentation,
* usage examples,
* design rationale,
* performance notes,
* related ADRs,
* related requirements.

---

## Acceptance Criteria

A public API specification is complete only when:

* behavior is fully documented,
* ownership is explicit,
* lifetime is specified,
* complexity is documented,
* allocation behavior is documented,
* thread safety is documented,
* error handling is documented,
* examples are provided,
* tests are specified,
* benchmarks are specified,
* traceability is complete.

The implementation agent should never need to infer public API behavior from surrounding code or documentation.

# Part 8 — Performance & Benchmark Requirements

Performance is a first-class engineering concern.

However, performance claims are **never** engineering requirements.

The PRD shall distinguish between:

* architectural performance goals,
* performance methodology,
* measured benchmark results.

The implementation shall optimize only where supported by reproducible evidence.

---

## Benchmark Philosophy

Benchmarks exist to answer engineering questions.

They do not exist to produce marketing numbers.

Every benchmark should test a hypothesis.

Examples:

* Does SIMD improve throughput?
* Does alignment reduce latency?
* Does the dispatch strategy introduce measurable overhead?
* Does memory layout improve cache locality?
* Does branchless execution reduce branch mispredictions?

Every benchmark must document the question being answered.

---

## Performance Contracts

Every performance-sensitive subsystem shall include a measurable performance contract.

The contract should define:

### Allocation Behavior

Specify whether the subsystem:

* allocates,
* reallocates,
* never allocates,
* allocates only during initialization.

Allocation behavior shall be verified through instrumentation.

---

### Expected Complexity

Specify:

* asymptotic complexity,
* expected practical complexity,
* dominant constants,
* scalability expectations.

---

### Cache Behavior

Document:

* expected locality,
* contiguous traversal,
* hot data,
* cold data,
* cache-line alignment,
* expected cache reuse.

Where cache behavior is important, justify the design.

---

### Branch Behavior

Document:

* predictable branches,
* unavoidable branches,
* branchless algorithms,
* dispatch behavior.

The PRD should explain why branch structure exists.

---

### SIMD Expectations

Specify:

* scalar baseline,
* vectorized path,
* ISA-specific implementations,
* fallback behavior.

Performance expectations should be expressed as methodology rather than target numbers.

---

## Benchmark Architecture

Design a dedicated benchmark subsystem.

Specify:

* repository location,
* benchmark organization,
* benchmark naming,
* benchmark ownership,
* benchmark dependencies,
* benchmark build targets.

Benchmarks should never depend upon testing infrastructure.

---

## Benchmark Categories

The PRD shall define benchmark categories.

Examples include:

### Microbenchmarks

Individual kernels.

Examples:

* filter
* scan
* aggregation
* comparisons
* arithmetic
* reductions

---

### Component Benchmarks

Subsystem interactions.

Examples:

* dispatch
* execution pipeline
* allocator
* scheduling

---

### End-to-End Benchmarks

Representative workloads.

These should measure complete execution paths rather than isolated functions.

---

### Regression Benchmarks

Benchmarks intended to detect performance regressions over time.

---

## Benchmark Methodology

Every benchmark shall document:

* purpose,
* workload,
* dataset,
* configuration,
* compiler,
* optimization level,
* ISA,
* runtime environment,
* warm-up procedure,
* iteration count,
* statistical methodology,
* collected metrics.

Benchmark methodology should be reproducible by another engineer.

---

## Environment Capture

Every benchmark artifact shall record:

* Git commit,
* repository cleanliness,
* compiler version,
* compiler flags,
* operating system,
* kernel version (where applicable),
* CPU model,
* core topology,
* available ISA extensions,
* memory capacity,
* benchmark timestamp.

This metadata becomes part of the benchmark artifact.

---

## Performance Ledger

Design a permanent performance ledger.

The ledger shall record benchmark history across releases.

Each benchmark entry should include:

* benchmark identifier,
* implementation version,
* execution environment,
* methodology version,
* measured metrics,
* notes,
* associated requirements.

Historical benchmark data should remain reproducible.

---

## PMU Integration

Where supported, define integration with hardware performance counters.

Examples:

* cycles,
* instructions,
* IPC,
* branches,
* branch misses,
* cache references,
* cache misses,
* TLB behavior,
* memory bandwidth,
* context switches.

The PRD shall specify:

* collection methodology,
* portability expectations,
* unsupported-platform behavior.

---

## Flamegraph Workflow

Specify:

* collection process,
* tooling,
* symbol requirements,
* artifact format,
* storage,
* publication.

Flamegraphs should accompany performance investigations rather than replace quantitative benchmarking.

---

## Benchmark Validation

Every benchmark should include validation.

Examples:

* correctness verification,
* invariant checks,
* deterministic outputs,
* reference comparisons.

Performance measurements without correctness validation are invalid.

---

## Performance Regression Policy

The PRD shall define how regressions are detected.

Specify:

* comparison baseline,
* statistical thresholds,
* reporting,
* investigation workflow,
* approval process.

Performance regressions should become visible during continuous integration where practical.

---

## Benchmark Traceability

Every benchmark shall reference:

* associated requirements,
* associated modules,
* associated APIs,
* associated ADRs,
* associated release milestones.

Every measurable engineering assumption should have a corresponding benchmark.

---

## Benchmark Documentation

Each benchmark shall generate documentation including:

* engineering objective,
* implementation notes,
* workload description,
* reproducibility instructions,
* interpretation guidance,
* limitations.

Documentation should explain *what* was measured and *why*, not merely present numbers.

---

## Acceptance Criteria

The performance architecture is complete only when:

* every performance-sensitive subsystem has a documented performance contract,
* every benchmark has a defined purpose,
* benchmark methodology is reproducible,
* environment capture is specified,
* PMU methodology is documented,
* flamegraph workflow is documented,
* performance ledger structure is defined,
* regression policy is documented,
* benchmark traceability is complete,
* no performance claim can exist without a documented benchmark methodology.

# Part 9 — Testing & Validation Requirements

Testing is a first-class engineering discipline.

The repository shall never treat testing as an activity performed after implementation.

Every architectural requirement shall have a corresponding validation strategy.

Implementation is not complete until every documented requirement has been objectively verified.

---

## Testing Philosophy

Testing exists to prove engineering correctness.

It is not intended solely to increase code coverage.

The primary objectives are:

* verify correctness,
* preserve invariants,
* prevent regressions,
* validate assumptions,
* verify contracts,
* ensure deterministic behavior.

Every test should answer an explicit engineering question.

---

## Validation Hierarchy

Validation shall occur at multiple levels.

At minimum:

```text
Requirements
        ↓
Architecture
        ↓
Module Contracts
        ↓
Public APIs
        ↓
Internal Components
        ↓
Integration
        ↓
Benchmarks
        ↓
Release Validation
```

Each level should verify a different class of engineering requirements.

---

## Testing Categories

The PRD shall define dedicated testing categories.

### Unit Tests

Purpose:

Verify individual functions and classes in isolation.

Unit tests shall validate:

* expected outputs,
* edge cases,
* invalid inputs,
* invariants,
* ownership semantics,
* lifetime guarantees.

---

### Integration Tests

Purpose:

Verify interaction between modules.

Examples:

* dispatch → kernel
* allocator → execution
* SIMD abstraction → kernel
* benchmark harness → execution pipeline

Integration tests should verify subsystem contracts rather than implementation details.

---

### Property-Based Testing

Every deterministic algorithm should define important mathematical or logical properties.

Examples:

* associativity
* commutativity
* idempotence
* ordering preservation
* selection correctness

The PRD should specify which properties are tested and why.

---

### Invariant Testing

Every documented invariant shall have at least one dedicated validation test.

Examples:

* column lengths remain equal
* selection vectors remain valid
* memory ownership never changes unexpectedly
* alignment guarantees hold
* dispatch tables remain consistent

No invariant should exist without verification.

---

### Regression Testing

Every resolved defect shall receive a regression test.

Regression tests should remain permanently in the repository unless explicitly deprecated.

---

### Differential Testing

Where practical, compare Quiver against trusted reference implementations.

Examples:

* scalar reference path
* alternative algorithms
* mathematically equivalent implementations

The purpose is to detect semantic divergence.

---

### Fuzz Testing

The PRD shall identify components appropriate for fuzz testing.

Examples:

* parsers
* dispatch logic
* API boundaries
* configuration handling

Specify:

* fuzz targets,
* corpus management,
* sanitizers,
* crash reporting.

---

### Stress Testing

Stress tests should verify:

* large datasets,
* boundary conditions,
* repeated execution,
* long-duration stability,
* resource exhaustion.

Stress testing should reveal implementation weaknesses rather than benchmark performance.

---

## Test Specification Template

Every test suite shall include:

### Purpose

Engineering question being answered.

---

### Scope

Subsystems validated.

---

### Inputs

Data supplied.

---

### Expected Outputs

Observable behavior.

---

### Requirements Covered

Referenced REQ identifiers.

---

### Invariants Covered

Referenced invariants.

---

### Failure Conditions

Precisely define failure.

---

### Repeatability

State whether deterministic execution is required.

---

## Requirement Traceability

Every requirement introduced by the PRD shall map to at least one validation artifact.

Examples:

```text
REQ-KERNEL-014
        ↓
Unit Test

REQ-DISPATCH-003
        ↓
Integration Test

REQ-SIMD-012
        ↓
Benchmark Validation

REQ-MEM-009
        ↓
Invariant Test
```

No requirement should exist without validation.

---

## Code Coverage Philosophy

Coverage is a diagnostic metric.

It is not the primary quality metric.

Prioritize:

* requirement coverage,
* invariant coverage,
* contract coverage,
* branch coverage where meaningful.

Avoid writing tests solely to increase numerical coverage.

---

## Continuous Validation

Continuous integration shall automatically execute:

* unit tests,
* integration tests,
* regression tests,
* invariant tests,
* sanitizer builds,
* static analysis.

Benchmark execution may be performed separately where runtime constraints exist.

---

## Sanitizer Strategy

Specify required validation under:

* AddressSanitizer
* UndefinedBehaviorSanitizer
* ThreadSanitizer (where applicable)
* LeakSanitizer (where applicable)

Every supported sanitizer configuration shall become part of release validation.

---

## Static Analysis

Specify required static analysis tooling.

Document:

* warning policy,
* compiler diagnostics,
* linting,
* formatting,
* prohibited constructs.

Static analysis complements testing rather than replacing it.

---

## Failure Reporting

Every failed validation should produce diagnostics sufficient to reproduce the issue.

Diagnostic reports should include:

* failing requirement identifiers,
* module,
* API,
* input,
* expected behavior,
* observed behavior,
* environment.

---

## Release Validation

No release shall be approved until:

* all required tests pass,
* sanitizers pass,
* static analysis passes,
* documented invariants hold,
* benchmark validation completes,
* documentation remains synchronized,
* traceability remains complete.

Release readiness should be objectively verifiable.

---

## Acceptance Criteria

The testing architecture is complete only when:

* every requirement has a validation artifact,
* every invariant has a dedicated test,
* every public API is validated,
* integration boundaries are tested,
* fuzz targets are documented,
* regression strategy is defined,
* sanitizer strategy is specified,
* release validation is documented,
* traceability between requirements and tests is complete,
* implementation quality can be evaluated objectively without relying on subjective judgment.

# Part 10 — Documentation & Architectural Decision Record (ADR) Requirements

Documentation is a first-class engineering artifact.

It shall evolve together with the implementation.

Documentation is not generated after the system is complete.

It is part of the system.

Every engineering decision, subsystem, API, benchmark, and release shall have corresponding documentation.

The implementation agent shall never invent undocumented behavior.

If implementation requires undocumented assumptions, the PRD is incomplete.

---

## Documentation Philosophy

Documentation exists to:

* preserve engineering intent,
* communicate architecture,
* justify design decisions,
* enable maintenance,
* support external contributors,
* provide historical traceability,
* explain tradeoffs,
* prevent architectural drift.

Documentation should answer:

* What exists?
* Why does it exist?
* Why was it designed this way?
* What alternatives were rejected?
* How should it evolve?

---

## Documentation Hierarchy

The repository shall define a structured documentation hierarchy.

At minimum include:

```text id="5jmfz1"
docs/
    architecture/
    api/
    adr/
    benchmarks/
    design/
    examples/
    guides/
    internals/
    performance/
    releases/
    testing/
```

Each directory shall have:

* purpose,
* ownership,
* maintenance responsibilities,
* expected contents.

---

## Documentation Ownership

Every subsystem shall own its documentation.

Documentation ownership shall mirror implementation ownership.

For every module specify:

* architecture documentation,
* API documentation,
* benchmark documentation,
* testing documentation,
* examples,
* design rationale.

No subsystem shall exist without corresponding documentation.

---

## Architecture Documentation

Every architectural subsystem shall include:

### Purpose

Why the subsystem exists.

---

### Responsibilities

Engineering responsibilities.

---

### Non-responsibilities

Explicit scope boundaries.

---

### Interfaces

Public interactions.

---

### Dependencies

Incoming and outgoing dependencies.

---

### Lifecycle

Initialization, execution, shutdown.

---

### Memory Model

Ownership and layout.

---

### Threading Model

Concurrency expectations.

---

### Performance Notes

Engineering rationale behind performance-sensitive decisions.

---

### Failure Modes

Expected and unexpected behavior.

---

### Related Requirements

Referenced REQ identifiers.

---

### Related ADRs

Referenced Architectural Decision Records.

---

## API Documentation

Every public API shall generate documentation containing:

* overview,
* signature,
* ownership,
* lifetime,
* preconditions,
* postconditions,
* examples,
* complexity,
* thread safety,
* allocation behavior,
* related requirements,
* related ADRs.

API documentation should never duplicate implementation comments.

---

## Benchmark Documentation

Every benchmark shall include:

* engineering objective,
* benchmark identifier,
* methodology,
* workload,
* environment,
* reproducibility instructions,
* collected metrics,
* interpretation guidance,
* limitations.

Benchmark documentation shall explain engineering conclusions rather than merely presenting numbers.

---

## Example Documentation

Every public subsystem shall include examples.

Examples should progress from:

* minimal,
* typical,
* advanced,
* edge-case usage.

Examples shall compile and remain synchronized with the implementation.

---

## Contribution Documentation

Document:

* repository structure,
* coding standards,
* workflow,
* testing expectations,
* benchmark expectations,
* documentation expectations,
* pull request process,
* review standards.

Contributors should understand engineering expectations before modifying the repository.

---

# Architectural Decision Records (ADRs)

Every architecturally significant decision shall receive an ADR.

Implementation shall follow approved ADRs.

Architectural changes require updating ADRs.

---

## ADR Philosophy

ADRs preserve engineering intent.

Future contributors should understand:

* why a decision was made,
* what alternatives existed,
* why alternatives were rejected,
* what assumptions influenced the decision,
* when reconsideration is appropriate.

---

## ADR Repository

Store ADRs under:

```text id="wl4a2z"
docs/adr/
```

Maintain an index.

Assign permanent identifiers.

Example:

```text id="6byz7h"
ADR-001
ADR-002
ADR-003
```

Identifiers shall never change.

---

## ADR Template

Every ADR shall include:

### Identifier

Permanent identifier.

---

### Title

Concise summary.

---

### Status

Examples:

* Proposed
* Accepted
* Superseded
* Deprecated

---

### Context

Engineering background.

---

### Problem Statement

Problem requiring a decision.

---

### Constraints

Relevant engineering constraints.

---

### Alternatives Considered

Document every realistic alternative.

---

### Tradeoff Analysis

Advantages and disadvantages.

---

### Decision

Chosen solution.

---

### Rationale

Why this solution was selected.

---

### Consequences

Positive and negative implications.

---

### Future Reconsideration Criteria

Conditions under which the ADR should be revisited.

---

### Related Requirements

Referenced REQ identifiers.

---

### Related Modules

Affected architectural modules.

---

## ADR Coverage

At minimum create ADRs for:

* repository architecture,
* ISA abstraction,
* dispatch strategy,
* memory ownership,
* allocator strategy,
* SIMD organization,
* benchmark methodology,
* API philosophy,
* threading model,
* release strategy.

The PRD shall identify all required ADRs before implementation begins.

---

## Documentation Traceability

Every documentation artifact shall reference:

* associated modules,
* associated APIs,
* associated requirements,
* associated ADRs,
* associated milestones.

Documentation shall participate in the repository-wide traceability model.

---

## Documentation Maintenance

Documentation shall evolve together with implementation.

Every implementation change affecting:

* architecture,
* APIs,
* behavior,
* benchmarks,
* testing,
* releases,

must update corresponding documentation before the change is considered complete.

Documentation debt is prohibited.

---

## Documentation Review

Every documentation artifact shall undergo technical review.

Review criteria include:

* correctness,
* consistency,
* completeness,
* traceability,
* terminology,
* synchronization with implementation.

---

## Release Documentation

Every release shall include:

* release summary,
* completed requirements,
* completed milestones,
* benchmark changes,
* API changes,
* ADR changes,
* known limitations,
* migration notes (if applicable).

Release documentation shall provide historical continuity.

---

## Acceptance Criteria

The documentation architecture is complete only when:

* every subsystem has dedicated documentation,
* every public API has reference documentation,
* every benchmark has methodology documentation,
* every architectural decision has an ADR,
* every ADR is indexed,
* documentation ownership is explicit,
* documentation traceability is complete,
* documentation evolves with implementation,
* release documentation requirements are defined,
* implementation can be understood without relying on undocumented engineering knowledge.

# Part 11 — Milestones, Traceability & Release Gates

Implementation shall proceed through a predefined sequence of engineering milestones.

Milestones are engineering contracts.

They are not merely progress checkpoints.

Each milestone shall leave the repository in a stable, releasable, fully validated state.

Implementation shall never proceed by accumulating partially complete work across multiple milestones.

Every milestone should represent a coherent increment of engineering capability.

---

## Milestone Philosophy

Each milestone shall:

* introduce a logically complete capability,
* maintain repository stability,
* preserve build integrity,
* preserve documentation integrity,
* preserve benchmark reproducibility,
* preserve test completeness.

No milestone should leave unfinished architecture behind.

Architectural work shall always precede implementation work.

---

## Milestone Specification Template

Every milestone shall include:

### Milestone Identifier

Example:

```text id="cxu8n5"
M0
M1
M2
...
M15
```

---

### Objective

Describe the engineering capability introduced.

---

### Scope

Specify exactly what is included.

Explicitly identify what is excluded.

---

### Dependencies

Document:

* required predecessor milestones,
* required completed modules,
* required ADRs,
* required documentation.

---

### Deliverables

Enumerate every required artifact.

Examples:

* source files,
* tests,
* benchmarks,
* documentation,
* diagrams,
* examples,
* CI updates.

---

### Repository Changes

Document:

* files created,
* files modified,
* modules introduced,
* APIs added,
* documentation added.

Repository evolution shall be deterministic.

---

### Requirements Implemented

List every REQ identifier completed by the milestone.

No requirement should be implemented outside an assigned milestone.

---

### ADRs Implemented

Reference the ADRs realized by the milestone.

---

### Tests Required

Specify:

* unit tests,
* integration tests,
* invariant tests,
* regression tests,
* fuzz tests,
* sanitizer validation.

Testing shall complete within the milestone.

---

### Benchmarks Required

Specify:

* benchmark suites,
* methodology,
* generated artifacts,
* validation requirements.

Benchmarks shall accompany implementation rather than follow it.

---

### Documentation Required

Specify:

* architecture updates,
* API documentation,
* benchmark documentation,
* examples,
* ADR updates,
* release notes.

Documentation shall complete with implementation.

---

### Acceptance Criteria

Define objective engineering completion criteria.

Examples:

* all REQs satisfied,
* all tests pass,
* sanitizers pass,
* benchmarks execute,
* documentation complete,
* CI passes,
* traceability updated.

Subjective completion criteria are prohibited.

---

### Risks

Identify:

* engineering risks,
* implementation risks,
* dependency risks,
* schedule risks.

Document mitigation strategies.

---

## Repository Evolution

Every milestone shall preserve a releasable repository.

The repository shall always:

* compile,
* pass validation,
* satisfy documented contracts,
* remain internally consistent.

Broken intermediate states are prohibited.

---

## Traceability Architecture

Every engineering artifact shall participate in a complete traceability model.

The traceability chain shall be:

```text id="3s91u0"
Research
        ↓
Opportunity Analysis
        ↓
Design Charter
        ↓
Requirement
        ↓
ADR
        ↓
Architecture
        ↓
Module
        ↓
Public API
        ↓
Implementation
        ↓
Tests
        ↓
Benchmarks
        ↓
Documentation
        ↓
Milestone
        ↓
Release
```

Every node shall reference adjacent nodes.

No artifact should exist outside the traceability model.

---

## Requirement Traceability

Every requirement shall document:

* originating document,
* owning subsystem,
* implementing module,
* associated ADR,
* associated milestone,
* validating tests,
* validating benchmarks,
* documentation references,
* release version.

Traceability shall be bidirectional.

---

## Implementation Dependency Graph

The PRD shall define a complete implementation dependency graph.

For every module identify:

* prerequisites,
* dependents,
* parallel implementation opportunities,
* blocking relationships.

The dependency graph shall be acyclic.

An autonomous implementation agent should derive implementation order directly from this graph.

---

## Release Gates

Every milestone shall terminate with a formal release gate.

Release gates verify engineering readiness.

A milestone may not be considered complete until its release gate succeeds.

---

### Release Gate Checklist

Verify:

* repository builds successfully,
* required modules implemented,
* required APIs implemented,
* tests pass,
* benchmarks execute,
* sanitizers pass,
* documentation synchronized,
* ADRs updated,
* traceability complete,
* release notes prepared.

Failure of any item prevents milestone completion.

---

## Version Progression

The PRD shall define planned repository evolution.

At minimum specify:

* v0.1
* v0.2
* v0.3
* v0.4
* v1.0

Each release shall document:

* completed milestones,
* completed requirements,
* API maturity,
* benchmark maturity,
* documentation maturity,
* known limitations.

---

## Continuous Integration Gates

Every pull request shall satisfy:

* formatting,
* linting,
* compilation,
* unit tests,
* integration tests,
* sanitizers,
* documentation validation,
* benchmark verification where applicable.

CI requirements shall be defined before implementation begins.

---

## Autonomous Implementation Support

The milestone architecture shall enable long-running autonomous implementation.

Every implementation task should already know:

* prerequisites,
* deliverables,
* files affected,
* tests required,
* benchmarks required,
* documentation required,
* acceptance criteria.

The implementation agent should never decide sequencing independently.

---

## Completion Standard

The milestone architecture is complete only when:

* every requirement belongs to exactly one milestone,
* every milestone has objective acceptance criteria,
* implementation order is deterministic,
* dependency graphs are complete,
* traceability is bidirectional,
* release gates are fully specified,
* repository evolution is deterministic,
* autonomous implementation can proceed milestone-by-milestone without architectural clarification.

# Part 12 — AI Implementation Contract

The Engineering PRD is being written with the explicit assumption that implementation will be performed by one or more autonomous AI software engineering agents.

Accordingly, the PRD shall function as a complete implementation contract.

The implementation phase must become an engineering exercise.

It must never become an architecture exercise.

The implementation agent shall implement architecture.

It shall not invent architecture.

---

## Primary Contract

Assume implementation may proceed for days or weeks with minimal or no human supervision.

The implementation agent shall never need to determine:

* project goals,
* repository organization,
* subsystem responsibilities,
* public APIs,
* ownership semantics,
* memory layouts,
* threading models,
* synchronization strategies,
* benchmark methodology,
* testing strategy,
* documentation organization,
* release sequencing.

Every architecturally significant decision shall already exist within the PRD.

---

## Scope of AI Autonomy

The implementation agent is authorized to make only local implementation decisions.

Examples include:

* variable names,
* helper function decomposition,
* statement ordering,
* private implementation details,
* formatting,
* comments,
* local refactoring,
* compiler-specific optimizations that preserve documented behavior.

The implementation agent is **not** authorized to:

* redesign modules,
* merge responsibilities,
* split responsibilities,
* redefine APIs,
* change ownership,
* modify public contracts,
* alter documented algorithms,
* change benchmark methodology,
* reinterpret requirements,
* weaken invariants,
* remove documentation requirements.

---

## Architectural Authority

The following documents constitute the complete architectural authority:

1. Literature Review
2. Opportunity Analysis
3. Design Charter
4. Engineering PRD

If implementation encounters ambiguity, the implementation agent shall:

1. Search the PRD.
2. Search related ADRs.
3. Search referenced requirements.
4. Search related module specifications.

If ambiguity remains, implementation shall stop and report the missing specification.

The implementation agent shall never resolve architectural ambiguity independently.

---

## Requirement Fidelity

Every implementation artifact shall trace directly to documented requirements.

No source file shall introduce behavior lacking a corresponding requirement.

No feature shall exist without:

* documented purpose,
* documented ownership,
* documented acceptance criteria.

---

## File-Level Responsibilities

Before implementing any file, the implementation agent shall know:

* why the file exists,
* owning subsystem,
* dependencies,
* public interfaces,
* associated requirements,
* associated ADRs,
* associated tests,
* associated benchmarks,
* associated documentation.

Implementation should never begin from an empty file without predefined intent.

---

## Autonomous Milestone Execution

The implementation agent shall complete milestones sequentially.

Each milestone shall include:

* implementation,
* testing,
* benchmarking,
* documentation,
* traceability updates,
* release validation.

The implementation agent shall not skip milestone requirements.

No milestone shall remain partially complete.

---

## Self-Validation

Before marking any task complete, the implementation agent shall verify:

* documented requirements satisfied,
* implementation consistent with ADRs,
* invariants preserved,
* tests written,
* benchmarks written where required,
* documentation updated,
* traceability updated,
* release gate satisfied.

Implementation completion shall require engineering validation rather than code generation alone.

---

## Architectural Drift Prevention

The implementation agent shall actively detect architectural drift.

Examples include:

* undocumented modules,
* undocumented APIs,
* duplicated responsibilities,
* inconsistent terminology,
* undocumented ownership,
* undocumented dependencies,
* undocumented benchmarks.

Architectural drift shall be treated as an implementation defect.

---

## Traceability Enforcement

Every implementation artifact shall reference:

* REQ identifiers,
* ADR identifiers,
* owning module,
* milestone,
* release.

Conversely, every requirement shall identify:

* implementation files,
* validating tests,
* benchmarks,
* documentation.

Traceability shall be complete and bidirectional.

---

## Implementation Quality Standards

The implementation agent shall prioritize:

* correctness,
* determinism,
* maintainability,
* readability,
* explicitness,
* portability,
* benchmarkability,
* reproducibility.

Code shall not optimize for cleverness.

Code shall optimize for engineering quality.

---

## Refactoring Policy

Refactoring is permitted only if:

* public behavior remains unchanged,
* documented requirements remain satisfied,
* ADRs remain valid,
* benchmark methodology remains valid,
* traceability remains complete.

Architectural refactoring requires a PRD amendment rather than implementation discretion.

---

## Error Handling

When implementation encounters:

* conflicting requirements,
* missing requirements,
* contradictory ADRs,
* impossible constraints,
* undefined ownership,
* undocumented lifecycle,

implementation shall stop and produce a structured engineering report describing:

* affected subsystem,
* conflicting artifacts,
* engineering impact,
* required architectural clarification.

Silent assumption is prohibited.

---

## Continuous Consistency Checks

The implementation agent shall continuously verify:

* repository organization,
* dependency graph,
* ownership model,
* module boundaries,
* API consistency,
* documentation synchronization,
* benchmark synchronization,
* testing completeness.

Consistency checking shall occur throughout implementation rather than only at release time.

---

## Definition of Implementation Completion

Implementation is complete only when:

* every documented requirement is implemented,
* every ADR is realized,
* every milestone passes its release gate,
* every public API matches its specification,
* every documented invariant is enforced,
* every required benchmark exists,
* every required test exists,
* every documentation artifact is complete,
* repository traceability is complete,
* no undocumented behavior exists anywhere in the repository.

The implementation agent should reach completion without making a single architecturally significant decision independently.

If architectural invention becomes necessary at any point, the Engineering PRD has failed its purpose and must be revised before implementation continues.

# Part 13 — Final Self-Review Checklist

The Engineering PRD shall conclude with a comprehensive self-review.

This review is the final quality gate before implementation is permitted to begin.

Its purpose is to verify that the PRD is architecturally complete, internally consistent, implementation-ready, and capable of supporting long-running autonomous software engineering without requiring architectural invention.

The PRD is not complete until every checklist item has been explicitly evaluated.

---

# Review Philosophy

The purpose of this review is not proofreading.

The purpose is engineering validation.

Assume a senior systems architect is performing the final design review before authorizing implementation.

Every unanswered architectural question represents a defect in the PRD.

---

# Section 1 — Product Validation

Verify that:

* the PRD faithfully implements the Design Charter.
* the product mission has not drifted.
* no new product scope has been introduced.
* permanent non-goals remain enforced.
* target personas remain unchanged.
* anti-personas remain excluded.
* repository identity is consistent throughout the document.

---

# Section 2 — Architectural Completeness

Verify that every subsystem includes:

* Purpose
* Responsibilities
* Non-responsibilities
* Public interfaces
* Internal architecture
* Dependencies
* Dependents
* Lifecycle
* State model
* Invariants
* Failure modes
* Performance contract
* Threading contract
* Memory contract
* Test matrix
* Benchmark matrix
* Documentation requirements
* Acceptance criteria

No subsystem shall be exempt.

---

# Section 3 — Repository Completeness

Verify that:

* repository hierarchy is fully defined.
* every directory has ownership.
* every module has ownership.
* dependency graph is complete.
* dependency graph is acyclic.
* implementation ordering is deterministic.
* visibility rules are documented.
* repository evolution is specified.

---

# Section 4 — API Completeness

Verify that every public API specifies:

* purpose,
* signature,
* ownership,
* lifetime,
* preconditions,
* postconditions,
* side effects,
* complexity,
* allocation behavior,
* exception guarantees,
* thread safety,
* invalid input behavior,
* examples,
* associated requirements,
* associated tests.

No API behavior shall remain undocumented.

---

# Section 5 — Performance Completeness

Verify that:

* every performance-sensitive subsystem has a performance contract.
* every benchmark has an engineering objective.
* benchmark methodology is documented.
* environment capture is specified.
* PMU methodology is specified.
* flamegraph workflow is documented.
* performance ledger is specified.
* regression policy exists.
* benchmark traceability is complete.

No performance claim shall exist without methodology.

---

# Section 6 — Testing Completeness

Verify that:

* every requirement has validation.
* every invariant has validation.
* every public API has tests.
* every subsystem has integration tests.
* fuzz targets are defined where appropriate.
* regression strategy is documented.
* sanitizer strategy is documented.
* release validation is documented.
* requirement coverage is complete.

No requirement shall exist without verification.

---

# Section 7 — Documentation Completeness

Verify that:

* every subsystem has architecture documentation.
* every public API has reference documentation.
* every benchmark has documentation.
* every ADR exists.
* ADR index is complete.
* contribution documentation exists.
* release documentation requirements exist.
* traceability documentation is complete.

Documentation shall be synchronized with architecture.

---

# Section 8 — ADR Validation

Verify that every major engineering decision has an ADR covering:

* context,
* problem,
* constraints,
* alternatives,
* tradeoffs,
* selected solution,
* rationale,
* consequences,
* reconsideration criteria,
* related requirements.

No major engineering decision shall exist without an ADR.

---

# Section 9 — Traceability Validation

Verify complete traceability from:

```text
Research
        ↓
Opportunity Analysis
        ↓
Design Charter
        ↓
Requirements
        ↓
ADRs
        ↓
Architecture
        ↓
Modules
        ↓
Public APIs
        ↓
Implementation
        ↓
Tests
        ↓
Benchmarks
        ↓
Documentation
        ↓
Milestones
        ↓
Releases
```

Every artifact shall be reachable through this chain.

No orphaned artifacts shall exist.

---

# Section 10 — Milestone Validation

Verify that:

* every requirement belongs to exactly one milestone.
* milestone dependencies are complete.
* milestone acceptance criteria are objective.
* release gates are specified.
* repository remains releasable after every milestone.
* implementation ordering is deterministic.

---

# Section 11 — Autonomous Implementation Readiness

Evaluate whether an autonomous implementation agent could complete the repository without making architecturally significant decisions.

Confirm that the implementation agent would **never** need to determine:

* repository organization,
* module boundaries,
* ownership,
* APIs,
* lifecycle,
* algorithms,
* threading,
* synchronization,
* benchmark methodology,
* testing strategy,
* documentation structure,
* release sequencing.

If any answer is "yes," identify the missing specification and revise the PRD.

---

# Section 12 — Internal Consistency

Verify that:

* terminology is consistent.
* diagrams match text.
* examples match APIs.
* APIs match architecture.
* architecture matches requirements.
* requirements match milestones.
* milestones match releases.
* documentation references remain valid.
* ADR references remain valid.
* requirement identifiers remain unique.
* no contradictory statements exist.

---

# Section 13 — Engineering Quality

Review the document against the following principles:

* correctness before optimization,
* measurement before optimization,
* explicitness before inference,
* determinism before convenience,
* maintainability before cleverness,
* simplicity unless complexity is justified,
* reproducibility over anecdotal evidence.

Every architectural decision should satisfy these principles.

---

# Section 14 — Scope Discipline

Confirm that the PRD has not expanded beyond the Design Charter.

Verify that it does **not** introduce:

* SQL parsing,
* storage engines,
* networking,
* distributed execution,
* authentication,
* unrelated utilities,
* speculative features,
* undocumented extensions.

Scope expansion requires a Design Charter amendment, not a PRD change.

---

# Section 15 — Final Approval Criteria

The Engineering PRD is approved for implementation only if all of the following are true:

* every requirement is uniquely identified.
* every subsystem is fully specified.
* every API is completely documented.
* every architectural decision has an ADR.
* every requirement is traceable.
* every benchmark is specified.
* every test is specified.
* every documentation artifact is specified.
* every milestone has objective acceptance criteria.
* every release has objective completion criteria.
* every engineering assumption has been documented.
* no architecturally significant ambiguity remains.

---

# Final Certification

Conclude the PRD with a formal certification.

The certification shall state that, to the best of the architect's knowledge:

* the repository architecture is complete,
* the engineering design is internally consistent,
* implementation sequencing is fully specified,
* autonomous implementation may proceed,
* any future architectural changes require amendment of the governing design documents rather than implementation-time invention.

The implementation phase is authorized only after this certification has been completed and all review criteria have been satisfied.
