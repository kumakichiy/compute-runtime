/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/memory_manager_fixture.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_memory_manager.h"

using namespace OCLRT;

void MemoryManagerWithCsrFixture::SetUp() {
    csr = new MockCommandStreamReceiver(this->executionEnvironment);
    memoryManager = new MockMemoryManager(executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    csr->tagAddress = &currentGpuTag;
    executionEnvironment.commandStreamReceivers.resize(1);
    executionEnvironment.commandStreamReceivers[0][0].reset(csr);
}

void MemoryManagerWithCsrFixture::TearDown() {
}
