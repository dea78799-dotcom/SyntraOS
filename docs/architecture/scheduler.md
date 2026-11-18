# SyntraOS Scheduler

Version: 0.1.0
Status: Design Phase

## 1. Overview

The Scheduler is responsible for deciding
which thread receives CPU execution time.

## 2. Main Responsibilities

- Select the next thread to execute.
- Manage thread scheduling states.
- Support priority-based scheduling.
- Handle thread switching.
- Coordinate with the timer subsystem.

## 3. Thread States

A thread may have the following states:

- READY: Waiting for CPU time.
- RUNNING: Currently executing.
- BLOCKED: Waiting for an event.
- TERMINATED: Execution has ended.

## 4. Scheduling Policy

SyntraOS initially plans to use
priority-based preemptive scheduling.

The scheduling policy may evolve
during development.

## 5. Architecture Support

The scheduler is designed to support:

- ARM64
- x86_64
- RISC-V 64

Architecture-specific context switching
will be implemented separately.

## 6. Current Status

The scheduler is not implemented yet.

This document describes the planned design.
