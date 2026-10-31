# Ariadne Testing — development backlog

This document tracks **proposed and unimplemented** functionality. Implementation
decisions, verified semantics, completed features, and CI evidence belong in
[testing_design.md](testing_design.md), **not** here. Upon completion of a task:
add the implementation and regression-test record to the design journal, then
remove or narrow the corresponding backlog item here. Do not write aspirational
APIs into the design journal.

## Scope and evidence (2026-10-30)

Source of repository scope:
[ariadne/decoupling/doc/decoupling/structure.md](https://github.com/ariadne-cps/ariadne/blob/decoupling/doc/decoupling/structure.md).
Surveyed the **main** branch of the 27 repositories in the dependency graph
other than **system, evolution, analysis, ariadne**, plus the separately
documented **configuration** dependency (28 repositories total). The prior
in-depth inventory of numeric, function and integration was incorporated and
cross-checked against their main test trees. The survey inspected test source
files, macro usage, and representative exceptional paths; it does **not**
claim tests were executed or that every test branch is reachable.

* numerical and algorithmic tests: numeric, interval, algebra, geometry,
  function, integration, linear-programming, nonlinear-programming,
  constraint-solving, equation-solving, symbolic, expression-set, set, paving,
  labelling, sat, smt, cdcl, bdd;
* infrastructure, I/O and visualization: foundation, logging, threading,
  utility, rendering, drawing;
* infrastructure without comparable C++ test executables: macro (testing
  macro definitions), python-common (no test sources found), configuration
  (Python unittest and CMake-script tests).

Baseline for absent/present capabilities: testing/main at
[dc9c514](https://github.com/ariadne-cps/testing/commit/dc9c514a52d7c9139a17824f6ac59ce927f49a82):
C++20, configuration-only dependency, Suite/Context/Reporter, indexed
RUN/CHECK events, evidence True/False/Indeterminate, equality, ADL same,
within, typed/any exception checks, fixture helper and deferred expression
macros. The proposals below must not introduce production dependencies on
any repository above configuration, or external test frameworks.

## P0 — Required for faithful migration

### D01. Generic, evidence-preserving relational and predicate checks

- [x] Add deferred, single-evaluation comparators for <, <=, >, >=, !=,
      and (where meaningful) explicit equality; preserve left-to-right
      operand evaluation and emit CHECK *before* computing operands.
- [x] Add unary/binary predicates with ADL, including negated relations, and
      retain/render both operands, the relation name and the resulting
      mathematical evidence. Provide thin optional macros for source spelling.
- [ ] Keep exact equality, domain-specific \`same\`/identity, mathematical
      equality, containment, intersection, refinement, consistency and
      numerical tolerance **separate**; expose generic predicate checking
      in Testing and allow domain-local named adapters instead of depending
      on numeric/function/interval/set.
- [x] Preserve signed/unsigned integral comparison safety and support
      heterogeneous user-defined operands; test indeterminate-valued
      relations and single-evaluation side effects.

**Evidence:** [interval/test_interval.cpp](https://github.com/ariadne-cps/interval/blob/main/tests/test_interval.cpp)
uses \`ARIADNE_TEST_BINARY_PREDICATE(intersect|disjoint|equal,...)\`;
[algebra/test_expansion.cpp](https://github.com/ariadne-cps/algebra/blob/main/tests/test_expansion.cpp)
uses \`same\` and its negation;
[function/test_taylor_model.cpp](https://github.com/ariadne-cps/function/blob/main/tests/test_taylor_model.cpp)
defines a local \`ARIADNE_TEST_REFINES\`;
[algebra/test_matrix.cpp](https://github.com/ariadne-cps/algebra/blob/main/tests/test_matrix.cpp)
tests norm bounds;
[numeric/test_number.cpp](https://github.com/ariadne-cps/numeric/blob/main/tests/test_number.cpp)
uses \`models\`. Generic \`prove(predicate(a,b))\` already expresses truth
but discards structured operands and predicate semantics.

### D02. Mathematical evidence intentions and outcome-policy matrix

- [x] Define explicit checks for "definitely true", "possibly true",
      "not possibly true", "indeterminate" and other *observed*
      logical status/value checks as supported by foundation. Distinguish
      a predicate's mathematical classification from whether a test met its
      **expected** classification.
- [x] Specify configurable exit-gate handling/reporting for inconclusive evidence
      without changing assertion outcomes; the default remains strict.
- [x] Separate test \`Inconclusive\` from a *successfully verified* domain
      result of \`UNKNOWN\` (e.g. expected SMT resource exhaustion).
      A test of an expected UNKNOWN reason should pass.
- [x] Write an explicit migration equivalence table for legacy
      \`ASSERT\`, \`EQUAL\`, \`EQUALS\`, \`SAME\`, \`COMPARE\`, \`WITHIN\`,
      \`CHECK\`, \`CONCEPT\`, \`FAIL\`, \`THROWS\` and local \`REFINES\`,
      including legacy treatment of indeterminate values; verify each
      conversion against its original domain semantics.

**Evidence:** [foundation/test_logical.cpp](https://github.com/ariadne-cps/foundation/blob/main/tests/test_logical.cpp)
exercises true/false/likely/unlikely/indeterminate logical values;
[linear-programming/test_linear_interior_point_method.cpp](https://github.com/ariadne-cps/linear-programming/blob/main/tests/test_linear_interior_point_method.cpp)
asserts \`is_indeterminate(...)\`;
[function/test_multifunction.cpp](https://github.com/ariadne-cps/function/blob/main/tests/test_multifunction.cpp)
does likewise; [smt/test_smt_neural_benchmarks.cpp](https://github.com/ariadne-cps/smt/blob/main/tests/test_smt_neural_benchmarks.cpp)
checks \`UNKNOWN\` together with \`RESOURCE_EXHAUSTED\`.
The old \`macro/testing.hpp\` only *warns* when \`ARIADNE_TEST_ASSERT\`
has indeterminate evidence, unlike Testing's \`Inconclusive\` outcome.

### D03. Parameterized suites, interfaces and fixture factories

- [x] Register one method over multiple runtime implementations of a
      common interface, with named implementation, fresh factory/clone
      semantics and optional subsets of methods per implementation.
- [x] Register one method over compile-time types, paradigms, rounding
      variants and runtime precision values. Each combination needs a
      stable, explicit display name and reproducible parameters.
- [x] Support fixture constructors taking \`Context&\` plus arbitrary
      factory-supplied arguments/state; do not force storing nullable
      context pointers. Distinguish *fresh per test* from explicitly
      requested shared setup/state, checking whether original suites
      have dependencies between methods.
- [x] Keep the two-level numeric progress \`case.check\` and case total;
      put variant names in case paths, not a third counter.
- [x] Ensure a failed fixture construction is an \`Error\` and does not
      silently skip every case.

**Evidence:** [integration/test_integrator.cpp](https://github.com/ariadne-cps/integration/blob/main/tests/test_integrator.cpp)
runs shared cases on several IntegratorInterface implementations with a
smaller Affine subset; [integration/test_bounder.cpp](https://github.com/ariadne-cps/integration/blob/main/tests/test_bounder.cpp)
clones BounderInterface and varies verbosity;
[nonlinear-programming/test_nonlinear_programming.cpp](https://github.com/ariadne-cps/nonlinear-programming/blob/main/tests/test_nonlinear_programming.cpp)
tests several solver implementations;
[equation-solving/test_equation_solvers.cpp](https://github.com/ariadne-cps/equation-solving/blob/main/tests/test_equation_solvers.cpp)
runs a common solver fixture over different algorithms;
[numeric/test_float_rounding.cpp](https://github.com/ariadne-cps/numeric/blob/main/tests/test_float_rounding.cpp)
runs several precision configurations;
[function/test_function_interface.cpp](https://github.com/ariadne-cps/function/blob/main/tests/test_function_interface.cpp)
runs Approximate/Validated/Effective type variants.


**Remaining:** explicit shared-fixture lifetime/setup semantics and concrete consumer migrations are separate work; D03 defaults to one fixture per registered case execution.

### D04. Scoped runtime diagnostics and parameter/row provenance

- [x] Associate zero or more nested diagnostic *labels* with the current
      case: algorithm, precision, iteration, row key, oracle filename,
      source line, input seed, rounding mode, subscenario, etc.; attach
      labels to each failure. Scoped RAII push/pop should survive exceptions.
- [x] Emit/flush the case/row context **before** potentially hanging work
      without requiring manual \`std::cout\` calls.
- [x] Provide non-asserting \`note\`, \`warning\`, \`value\`/inspection and
      elapsed-duration events; avoid treating printing as proof.
      Warnings must not be lost when output is muted; failure events remain
      authoritative and independent of the Logging singleton.
- [x] Allow selecting how much successful-case detail to display, without
      ever suppressing actionable failure diagnostics.

**Evidence:** [paving/test_grid_paving.cpp](https://github.com/ariadne-cps/paving/blob/main/tests/test_grid_paving.cpp)
has extensive \`ARIADNE_PRINT_TEST_CASE_TITLE\`/\`COMMENT\` and iterator
cases; [cdcl/test_cdcl_state.cpp](https://github.com/ariadne-cps/cdcl/blob/main/tests/test_cdcl_state.cpp)
uses manual phases; [smt/test_smt_solver.cpp](https://github.com/ariadne-cps/smt/blob/main/tests/test_smt_solver.cpp)
uses detailed \`std::cout\` markers;
[paving/test_affine_set_approximation.cpp](https://github.com/ariadne-cps/paving/blob/main/tests/test_affine_set_approximation.cpp)
manually measures durations; numeric test files annotate frozen TSV cases.


**Remaining follow-up:** richer key/value provenance attachments, source-specific TSV row helpers, and structured reporter export belong to later work; no domain-specific reader was added.

## P1 — Newly evidenced outside numeric/function/integration

### D05. Asynchronous progress and bounded concurrency checks

- [ ] Provide convenient checks for future/condition-variable/atomic
      progress with deadline, observed state and an informative timeout
      failure; make the waiting predicate deterministic when possible.
      **Partially implemented:** future and condition-variable waits; atomic
      progress, cross-thread synchronization and deadlines remain open.
- [ ] Define ownership, synchronization and aggregation for checks issued
      from worker threads; never allow concurrent writes to unprotected
      \`Context\`/Reporter state. Define reporting order and worker exception
      capture.
- [ ] Retain *process-level* CTest timeout/watchdog for genuinely blocked
      or deadlocked code; do not promise safe forced cancellation of a
      hung C++ thread from inside the same process.
- [ ] Preserve the distinction between timing-dependent test failure,
      scheduler slowdown and benchmark duration.

**Evidence:** [threading/test_thread_pool.cpp](https://github.com/ariadne-cps/threading/blob/main/tests/test_thread_pool.cpp),
[threading/test_buffer.cpp](https://github.com/ariadne-cps/threading/blob/main/tests/test_buffer.cpp),
[threading/test_thread_manager.cpp](https://github.com/ariadne-cps/threading/blob/main/tests/test_thread_manager.cpp),
[logging/test_concurrency.cpp](https://github.com/ariadne-cps/logging/blob/main/tests/test_concurrency.cpp),
[smt/test_smt_solver.cpp](https://github.com/ariadne-cps/smt/blob/main/tests/test_smt_solver.cpp).

### D06. External artifact and golden-regression support

- [x] Expose a generic test artifact reference (path, type, label and
      lifetime) and a failure attachment/diff path; support isolated
      per-case temporary output directories and predictable cleanup.
- [x] Keep PNG decoding and pixel-exact comparison in Rendering's local
      adapters; Testing records artifact locations, differences and
      checksums/metadata, not a graphics dependency.
- [x] Support checks on output-file existence and content/signature,
      with useful diagnostics. Never auto-regenerate or overwrite golden
      references when a test fails.

**Evidence:** [rendering/default_rendering_golden.hpp](https://github.com/ariadne-cps/rendering/blob/main/tests/default_rendering_golden.hpp)
already produces a pixel-exact comparison and difference image;
[rendering/test_default_rendering_regression.cpp](https://github.com/ariadne-cps/rendering/blob/main/tests/test_default_rendering_regression.cpp)
uses it; [labelling/test_figure.cpp](https://github.com/ariadne-cps/labelling/blob/main/tests/test_figure.cpp)
checks PNG dimensions; [drawing/test_box_draw.cpp](https://github.com/ariadne-cps/drawing/blob/main/tests/test_box_draw.cpp)
emits plots and checks drawing signatures.

### D07. Collection/tree/iterator comparison with location of first mismatch

- [x] Support element-wise checks for ranges/iterators/structured trees,
      including expected count, actual count, first differing index/path,
      and short value snippets, while evaluating inputs once.
- [x] Permit a custom comparator for semantic relations (e.g. cell
      equality or refinement); use adapters for domain-specific traversals
      rather than hard-coding GridTreePaving into Testing.

**Evidence:** [paving/test_grid_paving.cpp](https://github.com/ariadne-cps/paving/blob/main/tests/test_grid_paving.cpp)
uses \`ARIADNE_TEST_GRID_TREE_SUBPAVING_ITERATOR\` extensively;
[rendering/test_default_rendering_robustness.cpp](https://github.com/ariadne-cps/rendering/blob/main/tests/test_default_rendering_robustness.cpp)
checks image data and pixels.

### D08. Explicit domain-status expectations and alternative execution paths

- [x] Offer typed outcome expectations for allowed return statuses
      (SAT, UNSAT, UNKNOWN with reason, feasible/infeasible, etc.) and
      conditional witness/model postconditions, without mapping domain
      UNKNOWN automatically to \`Outcome::Inconclusive\`.
- [x] Provide a structured C++ facility for operations with several
      permitted exception/result alternatives, each possibly requiring
      postconditions. A warning is not a substitute for checking partial
      results or an explicit budget-exhaustion policy. Implemented through
      `Context::expect_alternative` with separately verified result and
      exception paths; consumers express permitted derived exceptions and
      partial-result/budget constraints in their explicit validators.

**Evidence:** [smt/test_smt_neural_benchmarks.cpp](https://github.com/ariadne-cps/smt/blob/main/tests/test_smt_neural_benchmarks.cpp)
tests resource-exhaustion reasons;
[sat/test_sat_solver.cpp](https://github.com/ariadne-cps/sat/blob/main/tests/test_sat_solver.cpp)
tests SAT/UNSAT and model existence;
[integration/test_integrator.cpp](https://github.com/ariadne-cps/integration/blob/main/tests/test_integrator.cpp)
checks IncompleteFlow/Bounds/TimeStep exceptions and partial flows;
[constraint-solving/test_constraint_solver.cpp](https://github.com/ariadne-cps/constraint-solving/blob/main/tests/test_constraint_solver.cpp)
checks witness validity conditionally.

## P1 — Cross-cutting migration work already identified

### D09. Abort-on-failure checks and exception-rich assertions

- [x] Add \`require\`/fatal checks for prerequisites (e.g. file open,
      valid parsed row, non-null pointer) and avoid unsafe continuation
      after a necessary assertion fails. Document \`Failed\` vs \`Error\`.
- [x] Provide exception type, message substring and exception payload
      checks, with argument evaluation announced before execution;
      retain \`expect_no_throw\` for operations without other assertions.
      **Implemented:** typed exception, message-substring and generic
      projected payload equality, including three-valued evidence.
- [ ] For old \`CONSTRUCT\`, \`NAMED_CONSTRUCT\`, \`EXECUTE\`, \`EVALUATE\`
      and \`ASSIGN\`, prefer normal C++ with \`expect_no_throw\`,
      value checks and structured notes. Avoid macros that obscure
      variable scope or retry expressions.

**Evidence:** [algebra/test_differential.cpp](https://github.com/ariadne-cps/algebra/blob/main/tests/test_differential.cpp),
[function/test_function.cpp](https://github.com/ariadne-cps/function/blob/main/tests/test_function.cpp),
[utility/test_array.cpp](https://github.com/ariadne-cps/utility/blob/main/tests/test_array.cpp).

### D10. Environment/configuration isolation and test fixture lifecycle

- [x] RAII backup/restore for per-case state (rounding modes, precision
      settings, singleton configuration, drawing backend, stream state).
      Preserve the original state even when the case throws.
      Generic value and getter/restorer guards implemented; domain adapters remain local.
- [ ] Define setup/teardown and share-or-recreate fixture semantics,
      without cross-case leakage, especially for solver warm starts.
- [ ] Avoid mandatory Logging dependency even when Logging-dependent
      consumers use verbosity variants.

**Evidence:** [numeric/test_float.cpp](https://github.com/ariadne-cps/numeric/blob/main/tests/test_float.cpp)
changes rounding modes; [integration/test_bounder.cpp](https://github.com/ariadne-cps/integration/blob/main/tests/test_bounder.cpp)
changes Logger verbosity; [drawing/test_constraint_set_draw.cpp](https://github.com/ariadne-cps/drawing/blob/main/tests/test_constraint_set_draw.cpp)
changes DrawerManager configuration;
[logging/test_logging.cpp](https://github.com/ariadne-cps/logging/blob/main/tests/test_logging.cpp)
works with singleton Logger state.

### D11. Compile-time checks and non-C++ test orchestration

- [ ] Add a lightweight naming/convenience API for compile-time
      concepts, traits and type equality, while keeping \`static_assert\`
      and C++ diagnostics authoritative.
- [ ] Support CMake/CTest registration of compile-only source targets,
      negative compilation fixtures where justified, Python unittest
      and CMake-script tests. Do not force them into a C++ \`Suite\`
      or add a Python runtime to Testing.
- [ ] Preserve CTest executable targets and testing labels; consider
      optional per-case discovery and filters only after reliable
      suite identity and parameterization.

**Evidence:** [foundation/test_paradigm.cpp](https://github.com/ariadne-cps/foundation/blob/main/tests/test_paradigm.cpp)
uses \`ARIADNE_TEST_CONCEPT\`;
[numeric/check_numeric.cpp](https://github.com/ariadne-cps/numeric/blob/main/tests/check_numeric.cpp)
is compiled separately as an object target;
[configuration/test_doc_markdown.py](https://github.com/ariadne-cps/configuration/blob/main/tests/test_doc_markdown.py)
and [configuration/test_submodule_paths.cmake](https://github.com/ariadne-cps/configuration/blob/main/tests/test_submodule_paths.cmake)
test non-C++ tooling.

### D12. Oracle data registration, reproducibility and dataset integrity

- [ ] Add optional metadata for immutable reference files, row key,
      iteration/repetition, deterministic seed, precision and input
      attributes to assist reproducibility. Validate absent, duplicated,
      malformed, extra and out-of-order rows through domain-local
      readers/checkers.
- [ ] Keep independent golden-data generators outside Testing and keep
      strict distinction between oracle verification and generating new
      fixtures. Never calculate expected results from code under test.
- [ ] Optional parameter/case selection must not silently reduce
      semantic coverage or disturb shared random-stream advancement.

**Evidence:** [numeric/tests/data/README-generators.md](https://github.com/ariadne-cps/numeric/blob/main/tests/data/README-generators.md)
and [README-sampling.md](https://github.com/ariadne-cps/numeric/blob/main/tests/data/README-sampling.md);
[numeric/test_rational.cpp](https://github.com/ariadne-cps/numeric/blob/main/tests/test_rational.cpp).

## P2 — Presentation and execution conveniences

### D13. Structured reporters, selection and CI integration

- [ ] Add JSON/JUnit output with structured classifications, nested
      contexts, timings and artifact paths, while keeping ConsoleReporter
      and streaming RUN/CHECK flushes authoritative for hang diagnosis.
- [ ] Add case filters/tags, per-implementation selection and optional
      per-case CTest listing. Avoid requiring Logging or third-party
      testing frameworks.
- [ ] Include a benchmark/elapsed-time note facility without inventing
      timing thresholds or claiming performance guarantees.

### D14. Property-based runs and diagnostics

- [ ] Explore seeded property checks over inputs with reproducible
      failing parameters, selective replay and optional shrinking.
      Random sampling alone is not a mathematical proof.
- [ ] Introduce only if existing randomized/oracle tests cannot be
      migrated faithfully via D04/D12; avoid hidden RNG reseeding or
      implicit sample-count changes.

## Suggested implementation and acceptance sequence

1. D01–D02: unit tests over exact and indeterminate values, ADL predicates,
   mathematical vs expected outcome, operand counts, and failure reports.
2. D03: three consumer-shaped regression fixtures exercising integration's
   multi-implementation subsets, numeric's precision matrix and function's
   template paradigm matrix, without linking those repositories.
3. D04/D09: row/subcase labels precede expensive work; fatal failures do
   not execute dependent assertions; exceptions restore diagnostic scopes.
4. D05: deterministic concurrency race/timeout tests, worker reports and
   CTest watchdog. Never rely on sleep alone for correctness.
5. D06/D07: artifact metadata and first-mismatch records using fake
   binary/sequence data; leave PNG/Grid implementation to adapters.
6. D08/D10/D11/D12: status alternatives, cleanup, external-target
   registration and frozen-oracle replay tests.
7. D13–D14 only after migration requirements and regression tests are green.

**Release gate for each item:** GCC/Clang/MSVC Debug and Release where CI
supports them, strict warning flags, sanitizers as applicable, no new
production dependencies, self-tests proving unchanged rigorous semantics,
and no loss of existing consumer test coverage. Changes to other repos
should follow their own branches; this backlog does not authorize a
migration or edit to those repos.

## Survey exclusions and uncertainties

* \`system\`, \`evolution\`, \`analysis\`, \`ariadne\` were deliberately
  excluded. The \`ariadne/decoupling\` structure is a dependency inventory,
  not a claim that every repository's \`main\` has the same test coverage.
* The graph describes \`configuration\` separately; it was inspected as
  a 28th repository for Python/CMake checks.
* \`macro\` exposes legacy test macros but no standalone tests in its
  \`main\` test tree; \`python-common\` has no dedicated test sources
  in its \`main\` tree.
* This is a source-level survey, not a compilation or execution audit.
  Priority indicates likely migration leverage, not benchmarked effort.
