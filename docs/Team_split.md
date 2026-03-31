# Team Split

## Purpose

This document defines an initial work split for a team of three people building `webserv`. It is meant to help parallel development, reduce overlap, and make integration safer.

This is a starting point, not a fixed contract. Ownership can be adjusted depending on strengths, progress, or blockers.

---

## Table of content

1. [Team goals](#1-team-goals)
2. [Proposed split](#2-proposed-split)
3. [Shared responsibilities](#3-shared-responsibilities)
4. [Interface agreements to define early](#4-interface-agreements-to-define-early)
5. [Suggested milestone ownership](#5-suggested-milestone-ownership)
    - [Milestone 1 - projects skeleton](#milestone-1--project-skeleton)
    - [Milestone 2 - minimal live server](#milestone-2--minimal-live-server)
    - [Milestone 3 - basic HTTP GET](#milestone-3--basic-http-get)
    - [Milestone 4 - config integration](#milestone-4--config-integration)
    - [Milestone 5 - POST upload + DELETE](#milestone-5--post-upload--delete)
    - [Milestone 6 - CGI](#milestone-6--cgi)
    - [Milestone 7 - hardening and evaluation prep](#milestone-7--hardening-and-evaluation-prep)
6. [Backup ownership](#6-backup-ownership)
7. [Workflow recommendations](#7-workflow-recommendations)
8. [Team anti patterns to avoid](#8-team-anti-patterns-to-avoid)
9. [Definition of a good split](#9-definition-of-a-good-split)
10. [To fill in later](#10-to-fill-in-later)

---

## 1. Team goals

The split should help the team:

* work in parallel,
* avoid duplicated effort,
* define clear interfaces,
* keep the project explainable by everyone,
* prevent “only one person understands this part” situations.

Important rule:
Even if each person has primary ownership of one subsystem, **all three teammates should understand the full request lifecycle and overall architecture**.

---

## 2. Proposed split

### Person 1 — Core networking / event loop

Primary focus: all connection and readiness-driven I/O logic.

#### Responsibilities

* listening socket creation
* non-blocking socket setup
* poll/equivalent integration
* accepting new clients
* client fd registration/removal
* client connection state machine
* non-blocking read/write flow
* safe connection cleanup
* handling disconnects and partial I/O correctly

#### Deliverables

* `Listener`
* `EventLoop`
* base `ClientConnection` lifecycle
* fd monitoring logic
* socket setup helpers

#### Integration dependencies

Needs stable interfaces from:

* request parser input/output contract
* response sending contract
* CGI fd integration contract

---

### Person 2 — HTTP + configuration

Primary focus: understanding requests and translating config into route behavior.

#### Responsibilities

* configuration file parser
* config validation
* server/location config models
* request line parsing
* header parsing
* request body metadata handling
* route matching
* allowed methods logic
* redirect logic
* error response logic
* response building helpers

#### Deliverables

* `ConfigParser`
* `ServerConfig`
* `LocationConfig`
* `HttpRequest`
* `HttpRequestParser`
* `Router`
* `HttpResponseBuilder`

#### Integration dependencies

Needs stable interfaces from:

* connection buffer handling
* filesystem/resource resolution
* CGI execution result format

---

### Person 3 — Filesystem + methods + CGI

Primary focus: turning routed requests into actual actions and content.

#### Responsibilities

* static file serving
* path resolution helpers
* index file handling
* autoindex generation
* upload handling
* DELETE implementation
* CGI execution setup
* CGI environment variables
* CGI pipe/process handling
* CGI output parsing
* method-specific integration tests

#### Deliverables

* `ResourceResolver`
* file serving helpers
* upload/delete handlers
* autoindex generator
* `CgiHandler`

#### Integration dependencies

Needs stable interfaces from:

* routed request/config result
* request body availability
* response builder contract
* event loop support for CGI-related fds if needed

---

## 3. Shared responsibilities

These areas must be shared across the whole team.

### Shared understanding required

All team members should be able to explain:

* project architecture
* request lifecycle
* config format
* non-blocking constraints from the subject
* GET / POST / DELETE behavior
* CGI flow at a high level
* how testing is being done

### Shared artifacts

The team should maintain together:

* `architecture.md`
* `request_lifecycle.md`
* `testing_checklist.md`
* config examples for evaluation
* README

### Shared review responsibility

Before merging major code:

* another teammate should review it,
* the owner should be able to explain it,
* at least one non-owner should also be able to explain the main idea.

---

## 4. Interface agreements to define early

Before parallel development starts, the team should agree on these exact interfaces.

### A. `HttpRequest`

Decide fields such as:

* method
* path
* query string
* version
* headers
* body
* content length
* body state

### B. `ClientConnection`

Decide what connection owns:

* input buffer
* output buffer
* parser state
* request object
* response object
* current lifecycle state
* timestamps/activity metadata
* CGI metadata if active

### C. Router result

Define what routing returns.
Possible content:

* matched server config
* matched location config
* resolved method permissions
* redirect info
* root/path info
* CGI mode
* upload permissions

### D. Response builder contract

Define what is passed in to build a response and what the output object contains.

### E. CGI result contract

Define how CGI communicates its result back to the rest of the server.

Without these agreements, integration will become painful.

---

## 5. Suggested milestone ownership

### Milestone 1 — project skeleton

**All together**

* folder structure
* base headers/classes
* Makefile
* coding style decisions
* config grammar draft

### Milestone 2 — minimal live server

**Person 1 lead**

* listen on port
* accept clients
* send fixed response

Support:

* Person 2 prepares basic request/response structures
* Person 3 prepares static content/demo files

### Milestone 3 — basic HTTP GET

**Person 2 + Person 3 lead**

* parse request line and headers
* route basic path
* resolve and serve static file
* return 404/405/500 correctly

Support:

* Person 1 ensures clean read/write lifecycle

### Milestone 4 — config integration

**Person 2 lead**

* config file parser
* server/location matching
* route behavior config

Support:

* Person 3 integrates filesystem behavior
* Person 1 integrates listener setup from config

### Milestone 5 — POST upload + DELETE

**Person 3 lead**

* upload destination handling
* write uploaded file
* DELETE behavior

Support:

* Person 2 validates request and route rules
* Person 1 ensures body transfer and response sending remain stable

### Milestone 6 — CGI

**Person 3 lead**

* CGI execution pipeline

Support:

* Person 1 handles non-blocking integration and fd management
* Person 2 handles request data mapping and response parsing rules

### Milestone 7 — hardening and evaluation prep

**All together**

* stress tests
* invalid request handling
* browser tests
* NGINX comparison
* documentation cleanup
* demo configs

---

## 6. Backup ownership

Each subsystem should have a backup owner.

Suggested backup mapping:

* Person 1 backed up by Person 2
* Person 2 backed up by Person 3
* Person 3 backed up by Person 1

This prevents one blocked teammate from stopping the project.

---

## 7. Workflow recommendations

### Keep branches small

Do not keep large isolated branches for too long.
Merge small working pieces often.

### Sync regularly

Suggested team sync topics:

* what was finished
* current blockers
* next integration point
* what changed in interfaces

### Demo often

After each milestone, run a short team demo:

* what works,
* what is still broken,
* what assumptions changed.

### Write tests as features are built

Do not postpone testing until the end.
Every subsystem should come with at least a few repeatable checks.

---

## 8. Team anti-patterns to avoid

* one person writing the “hard part” alone with no documentation
* merging undocumented interface changes
* testing only with browser clicks
* postponing integration too long
* building CGI before the base request lifecycle is stable
* unclear ownership of cleanup and error handling

---

## 9. Definition of a good split

A good split means:

* everyone has clear primary tasks,
* interfaces are known early,
* integration points are planned,
* everyone can still explain the project,
* no subsystem becomes a black box owned by only one person.

---

## 10. To fill in later

When the team is finalized, update this file with:

* actual names/logins,
* final ownership,
* backup ownership,
* milestone deadlines,
* current blockers,
* integration dates.

Example section to add later:

```md
## Current assignment
- Person 1: ...
- Person 2: ...
- Person 3: ...

## Current blockers
- ...

## Next merge target
- ...
```

This file should stay practical and be updated during the project, not only at the start.
