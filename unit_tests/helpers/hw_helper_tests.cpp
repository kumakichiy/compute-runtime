/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/string.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/hw_helper_tests.h"
#include "unit_tests/helpers/unit_test_helper.h"

#include <chrono>
#include <iostream>
#include <numeric>
#include <vector>

void HwHelperFixture::SetUp() {
    DeviceFixture::SetUp();
}
void HwHelperFixture::TearDown() {
    DeviceFixture::TearDown();
}

TEST_F(HwHelperTest, getReturnsValidHwHelperHw) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_NE(nullptr, &helper);
}

HWTEST_F(HwHelperTest, getBindingTableStateSurfaceStatePointerReturnsCorrectPointer) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    BINDING_TABLE_STATE bindingTableState[4];

    bindingTableState[2].getRawData(0) = 0x00123456;

    auto &helper = HwHelper::get(renderCoreFamily);

    auto pointer = helper.getBindingTableStateSurfaceStatePointer(bindingTableState, 2);
    EXPECT_EQ(0x00123456u, pointer);
}

HWTEST_F(HwHelperTest, getBindingTableStateSizeReturnsCorrectSize) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;

    auto &helper = HwHelper::get(renderCoreFamily);

    auto pointer = helper.getBindingTableStateSize();
    EXPECT_EQ(sizeof(BINDING_TABLE_STATE), pointer);
}

TEST_F(HwHelperTest, getBindingTableStateAlignementReturnsCorrectSize) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_NE(0u, helper.getBindingTableStateAlignement());
}

HWTEST_F(HwHelperTest, getInterfaceDescriptorDataSizeReturnsCorrectSize) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    auto &helper = HwHelper::get(renderCoreFamily);

    EXPECT_EQ(sizeof(INTERFACE_DESCRIPTOR_DATA), helper.getInterfaceDescriptorDataSize());
}

TEST_F(HwHelperTest, givenDebuggingInactiveWhenSipKernelTypeIsQueriedThenCsrTypeIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_NE(nullptr, &helper);

    auto sipType = helper.getSipKernelType(false);
    EXPECT_EQ(SipKernelType::Csr, sipType);
}

TEST_F(HwHelperTest, givenEngineTypeRcsWhenCsTraitsAreQueiredThenCorrectNameInTraitsIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_NE(nullptr, &helper);

    auto &csTraits = helper.getCsTraits(EngineType::ENGINE_RCS);
    EXPECT_STREQ("RCS", csTraits.name);
}

HWTEST_F(HwHelperTest, givenHwHelperWhenAskedForPageTableManagerSupportThenReturnCorrectValue) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(helper.isPageTableManagerSupported(hwInfoHelper.hwInfo), UnitTestHelper<FamilyType>::isPageTableManagerSupported(hwInfoHelper.hwInfo));
}

TEST(DwordBuilderTest, setNonMaskedBits) {
    uint32_t dword = 0;

    // expect non-masked bit 2
    uint32_t expectedDword = (1 << 2);
    dword = DwordBuilder::build(2, false, true, 0); // set 2nd bit
    EXPECT_EQ(expectedDword, dword);

    // expect non-masked bits 2 and 3
    expectedDword |= (1 << 3);
    dword = DwordBuilder::build(3, false, true, dword); // set 3rd bit with init value
    EXPECT_EQ(expectedDword, dword);
}

TEST(DwordBuilderTest, setMaskedBits) {
    uint32_t dword = 0;

    // expect masked bit 2
    uint32_t expectedDword = (1 << 2);
    expectedDword |= (1 << (2 + 16));
    dword = DwordBuilder::build(2, true, true, 0); // set 2nd bit (masked)
    EXPECT_EQ(expectedDword, dword);

    // expect masked bits 2 and 3
    expectedDword |= (1 << 3);
    expectedDword |= (1 << (3 + 16));
    dword = DwordBuilder::build(3, true, true, dword); // set 3rd bit (masked) with init value
    EXPECT_EQ(expectedDword, dword);
}

TEST(DwordBuilderTest, setMaskedBitsWithDifferentBitValue) {
    // expect only mask bit
    uint32_t expectedDword = 1 << (2 + 16);
    auto dword = DwordBuilder::build(2, true, false, 0);
    EXPECT_EQ(expectedDword, dword);

    // expect masked bits 3
    expectedDword = (1 << 3);
    expectedDword |= (1 << (3 + 16));
    dword = DwordBuilder::build(3, true, true, 0);
    EXPECT_EQ(expectedDword, dword);
}

using LriHelperTests = ::testing::Test;

HWTEST_F(LriHelperTests, givenAddressAndOffsetWhenHelperIsUsedThenProgramCmdStream) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    uint32_t address = 0x8888;
    uint32_t data = 0x1234;

    auto expectedLri = MI_LOAD_REGISTER_IMM::sInit();
    expectedLri.setRegisterOffset(address);
    expectedLri.setDataDword(data);

    auto lri = LriHelper<FamilyType>::program(&stream, address, data);

    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), stream.getUsed());
    EXPECT_EQ(lri, stream.getCpuBase());
    EXPECT_TRUE(memcmp(lri, &expectedLri, sizeof(MI_LOAD_REGISTER_IMM)) == 0);
}

using PipeControlHelperTests = ::testing::Test;

HWTEST_F(PipeControlHelperTests, givenPostSyncWriteTimestampModeWhenHelperIsUsedThenProperFieldsAreProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    uint64_t address = 0x1234567887654321;
    uint64_t immediateData = 0x1234;

    auto expectedPipeControl = PIPE_CONTROL::sInit();
    expectedPipeControl.setCommandStreamerStallEnable(true);
    expectedPipeControl.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP);
    expectedPipeControl.setAddress(static_cast<uint32_t>(address & 0x0000FFFFFFFFULL));
    expectedPipeControl.setAddressHigh(static_cast<uint32_t>(address >> 32));

    auto pipeControl = PipeControlHelper<FamilyType>::obtainPipeControlAndProgramPostSyncOperation(&stream, PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, address, immediateData);

    EXPECT_EQ(sizeof(PIPE_CONTROL), stream.getUsed());
    EXPECT_EQ(pipeControl, stream.getCpuBase());
    EXPECT_TRUE(memcmp(pipeControl, &expectedPipeControl, sizeof(PIPE_CONTROL)) == 0);
}

HWTEST_F(PipeControlHelperTests, givenPostSyncWriteImmediateDataModeWhenHelperIsUsedThenProperFieldsAreProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    uint64_t address = 0x1234567887654321;
    uint64_t immediateData = 0x1234;

    auto expectedPipeControl = PIPE_CONTROL::sInit();
    expectedPipeControl.setCommandStreamerStallEnable(true);
    expectedPipeControl.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA);
    expectedPipeControl.setAddress(static_cast<uint32_t>(address & 0x0000FFFFFFFFULL));
    expectedPipeControl.setAddressHigh(static_cast<uint32_t>(address >> 32));
    expectedPipeControl.setImmediateData(immediateData);

    auto pipeControl = PipeControlHelper<FamilyType>::obtainPipeControlAndProgramPostSyncOperation(&stream, PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, address, immediateData);

    EXPECT_EQ(sizeof(PIPE_CONTROL), stream.getUsed());
    EXPECT_EQ(pipeControl, stream.getCpuBase());
    EXPECT_TRUE(memcmp(pipeControl, &expectedPipeControl, sizeof(PIPE_CONTROL)) == 0);
}

TEST(HwInfoTest, givenHwInfoWhenIsCoreThenPlatformTypeIsCore) {
    HardwareInfo hwInfo;
    hwInfo.capabilityTable.isCore = true;
    auto platformType = getPlatformType(hwInfo);
    EXPECT_STREQ("core", platformType);
}

TEST(HwInfoTest, givenHwInfoWhenIsNotCoreThenPlatformTypeIsLp) {
    HardwareInfo hwInfo;
    hwInfo.capabilityTable.isCore = false;
    auto platformType = getPlatformType(hwInfo);
    EXPECT_STREQ("lp", platformType);
}

TEST(HwInfoTest, givenHwInfoWhenChosenEngineTypeQueriedThenDefaultIsReturned) {
    HardwareInfo hwInfo;
    hwInfo.capabilityTable.defaultEngineType = EngineType::ENGINE_RCS;
    auto engineType = getChosenEngineType(hwInfo);
    EXPECT_EQ(EngineType::ENGINE_RCS, engineType);
}

TEST(HwInfoTest, givenNodeOrdinalSetWhenChosenEngineTypeQueriedThenSetValueIsReturned) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.NodeOrdinal.set(EngineType::ENGINE_VECS);
    HardwareInfo hwInfo;
    hwInfo.capabilityTable.defaultEngineType = EngineType::ENGINE_RCS;
    auto engineType = getChosenEngineType(hwInfo);
    EXPECT_EQ(EngineType::ENGINE_VECS, engineType);
}

HWTEST_F(HwHelperTest, givenCreatedSurfaceStateBufferWhenNoAllocationProvidedThenUseArgumentsasInput) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    ExecutionEnvironment &ee = *pDevice->getExecutionEnvironment();
    auto gmmHelper = ee.getGmmHelper();

    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(sizeof(RENDER_SURFACE_STATE), helper.getRenderSurfaceStateSize());

    size_t size = 0x1000;
    SURFACE_STATE_BUFFER_LENGTH length;
    length.Length = static_cast<uint32_t>(size - 1);
    uint64_t addr = 0x2000;
    size_t offset = 0x1000;
    uint32_t pitch = 0x40;
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    helper.setRenderSurfaceStateForBuffer(ee, stateBuffer, size, addr, offset, pitch, nullptr, 0, type, true);

    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);
    EXPECT_EQ(length.SurfaceState.Depth + 1u, state->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, state->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, state->getHeight());
    EXPECT_EQ(pitch, state->getSurfacePitch());
    addr += offset;
    EXPECT_EQ(addr, state->getSurfaceBaseAddress());
    EXPECT_EQ(type, state->getSurfaceType());
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER), state->getMemoryObjectControlState());

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    size = 0x1003;
    length.Length = static_cast<uint32_t>(alignUp(size, 4) - 1);
    helper.setRenderSurfaceStateForBuffer(ee, stateBuffer, size, addr, 0, pitch, nullptr, 0, type, true);
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), state->getMemoryObjectControlState());
    EXPECT_EQ(length.SurfaceState.Depth + 1u, state->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, state->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, state->getHeight());

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    size = 0x1000;
    addr = 0x2001;
    length.Length = static_cast<uint32_t>(size - 1);
    helper.setRenderSurfaceStateForBuffer(ee, stateBuffer, size, addr, 0, pitch, nullptr, 0, type, true);
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), state->getMemoryObjectControlState());
    EXPECT_EQ(length.SurfaceState.Depth + 1u, state->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, state->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, state->getHeight());
    EXPECT_EQ(addr, state->getSurfaceBaseAddress());

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    size = 0x1005;
    length.Length = static_cast<uint32_t>(alignUp(size, 4) - 1);
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    helper.setRenderSurfaceStateForBuffer(ee, stateBuffer, size, addr, 0, pitch, nullptr, flags, type, true);
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER), state->getMemoryObjectControlState());
    EXPECT_EQ(length.SurfaceState.Depth + 1u, state->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, state->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, state->getHeight());
    EXPECT_EQ(addr, state->getSurfaceBaseAddress());

    alignedFree(stateBuffer);
}

HWTEST_F(HwHelperTest, givenCreatedSurfaceStateBufferWhenAllocationProvidedThenUseAllocationAsInput) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    ExecutionEnvironment &ee = *pDevice->getExecutionEnvironment();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &helper = HwHelper::get(renderCoreFamily);

    size_t size = 0x1000;
    SURFACE_STATE_BUFFER_LENGTH length;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    length.Length = static_cast<uint32_t>(allocSize - 1);
    GraphicsAllocation allocation(cpuAddr, gpuAddr, 0u, allocSize, 0, false);
    allocation.gmm = new Gmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), false);
    ASSERT_NE(nullptr, allocation.gmm);
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    helper.setRenderSurfaceStateForBuffer(ee, stateBuffer, size, addr, 0, pitch, &allocation, 0, type, true);
    EXPECT_EQ(length.SurfaceState.Depth + 1u, state->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, state->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, state->getHeight());
    EXPECT_EQ(pitch, state->getSurfacePitch() - 1u);
    EXPECT_EQ(gpuAddr, state->getSurfaceBaseAddress());

    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT, state->getCoherencyType());
    EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, state->getAuxiliarySurfaceMode());

    delete allocation.gmm;
    alignedFree(stateBuffer);
}

HWTEST_F(HwHelperTest, givenCreatedSurfaceStateBufferWhenGmmAndAllocationCompressionEnabledAnNonAuxDisabledThenSetCoherencyToGpuAndAuxModeToCompression) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    ExecutionEnvironment &ee = *pDevice->getExecutionEnvironment();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &helper = HwHelper::get(renderCoreFamily);

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    GraphicsAllocation allocation(cpuAddr, gpuAddr, 0u, allocSize, 0, false);
    allocation.gmm = new Gmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), false);
    ASSERT_NE(nullptr, allocation.gmm);
    allocation.gmm->isRenderCompressed = true;
    allocation.setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    helper.setRenderSurfaceStateForBuffer(ee, stateBuffer, size, addr, 0, pitch, &allocation, 0, type, false);
    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT, state->getCoherencyType());
    EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E, state->getAuxiliarySurfaceMode());

    delete allocation.gmm;
    alignedFree(stateBuffer);
}

HWTEST_F(HwHelperTest, givenCreatedSurfaceStateBufferWhenGmmCompressionEnabledAndAllocationDisabledAnNonAuxDisabledThenSetCoherencyToIaAndAuxModeToNone) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    ExecutionEnvironment &ee = *pDevice->getExecutionEnvironment();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &helper = HwHelper::get(renderCoreFamily);

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    GraphicsAllocation allocation(cpuAddr, gpuAddr, 0u, allocSize, 0, false);
    allocation.gmm = new Gmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), false);
    ASSERT_NE(nullptr, allocation.gmm);
    allocation.gmm->isRenderCompressed = true;
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    helper.setRenderSurfaceStateForBuffer(ee, stateBuffer, size, addr, 0, pitch, &allocation, 0, type, false);
    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT, state->getCoherencyType());
    EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, state->getAuxiliarySurfaceMode());

    delete allocation.gmm;
    alignedFree(stateBuffer);
}

HWTEST_F(HwHelperTest, givenCreatedSurfaceStateBufferWhenGmmCompressionDisabledAndAllocationEnabledAnNonAuxDisabledThenSetCoherencyToIaAndAuxModeToNone) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    ExecutionEnvironment &ee = *pDevice->getExecutionEnvironment();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &helper = HwHelper::get(renderCoreFamily);

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    GraphicsAllocation allocation(cpuAddr, gpuAddr, 0u, allocSize, 0, false);
    allocation.gmm = new Gmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), false);
    ASSERT_NE(nullptr, allocation.gmm);
    allocation.setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    helper.setRenderSurfaceStateForBuffer(ee, stateBuffer, size, addr, 0, pitch, &allocation, 0, type, false);
    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT, state->getCoherencyType());
    EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, state->getAuxiliarySurfaceMode());

    delete allocation.gmm;
    alignedFree(stateBuffer);
}

HWTEST_F(HwHelperTest, givenCreatedSurfaceStateBufferWhenGmmAndAllocationCompressionEnabledAnNonAuxEnabledThenSetCoherencyToIaAndAuxModeToNone) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    ExecutionEnvironment &ee = *pDevice->getExecutionEnvironment();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &helper = HwHelper::get(renderCoreFamily);

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    GraphicsAllocation allocation(cpuAddr, gpuAddr, 0u, allocSize, 0, false);
    allocation.gmm = new Gmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), false);
    ASSERT_NE(nullptr, allocation.gmm);
    allocation.gmm->isRenderCompressed = true;
    allocation.setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    helper.setRenderSurfaceStateForBuffer(ee, stateBuffer, size, addr, 0, pitch, &allocation, 0, type, true);
    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT, state->getCoherencyType());
    EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, state->getAuxiliarySurfaceMode());

    delete allocation.gmm;
    alignedFree(stateBuffer);
}

HWTEST_F(HwHelperTest, DISABLED_profilingCreationOfRenderSurfaceStateVsMemcpyOfCachelineAlignedBuffer) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    constexpr uint32_t maxLoop = 1000u;

    std::vector<std::chrono::time_point<std::chrono::high_resolution_clock>> timesCreate;
    timesCreate.reserve(maxLoop * 2);

    std::vector<std::chrono::time_point<std::chrono::high_resolution_clock>> timesMemCpy;
    timesMemCpy.reserve(maxLoop * 2);

    std::vector<int64_t> nanoDurationCreate;
    nanoDurationCreate.reserve(maxLoop);

    std::vector<int64_t> nanoDurationCpy;
    nanoDurationCpy.reserve(maxLoop);

    std::vector<void *> surfaceStates;
    surfaceStates.reserve(maxLoop);

    std::vector<void *> copyBuffers;
    copyBuffers.reserve(maxLoop);

    for (uint32_t i = 0; i < maxLoop; ++i) {
        void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
        ASSERT_NE(nullptr, stateBuffer);
        memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
        surfaceStates.push_back(stateBuffer);

        void *copyBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
        ASSERT_NE(nullptr, copyBuffer);
        copyBuffers.push_back(copyBuffer);
    }

    ExecutionEnvironment &ee = *pDevice->getExecutionEnvironment();
    auto &helper = HwHelper::get(renderCoreFamily);

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;

    for (uint32_t i = 0; i < maxLoop; ++i) {
        auto t1 = std::chrono::high_resolution_clock::now();
        helper.setRenderSurfaceStateForBuffer(ee, surfaceStates[i], size, addr, 0, pitch, nullptr, 0, type, true);
        auto t2 = std::chrono::high_resolution_clock::now();
        timesCreate.push_back(t1);
        timesCreate.push_back(t2);
    }

    for (uint32_t i = 0; i < maxLoop; ++i) {
        auto t1 = std::chrono::high_resolution_clock::now();
        memcpy_s(copyBuffers[i], sizeof(RENDER_SURFACE_STATE), surfaceStates[i], sizeof(RENDER_SURFACE_STATE));
        auto t2 = std::chrono::high_resolution_clock::now();
        timesMemCpy.push_back(t1);
        timesMemCpy.push_back(t2);
    }

    for (uint32_t i = 0; i < maxLoop; ++i) {
        std::chrono::duration<double> delta = timesCreate[i * 2 + 1] - timesCreate[i * 2];
        std::chrono::nanoseconds duration = std::chrono::duration_cast<std::chrono::nanoseconds>(delta);
        nanoDurationCreate.push_back(duration.count());

        delta = timesMemCpy[i * 2 + 1] - timesMemCpy[i * 2];
        duration = std::chrono::duration_cast<std::chrono::nanoseconds>(delta);
        nanoDurationCpy.push_back(duration.count());
    }

    sort(nanoDurationCreate.begin(), nanoDurationCreate.end());
    sort(nanoDurationCpy.begin(), nanoDurationCpy.end());

    double averageCreate = std::accumulate(nanoDurationCreate.begin(), nanoDurationCreate.end(), 0.0) / nanoDurationCreate.size();
    double averageCpy = std::accumulate(nanoDurationCpy.begin(), nanoDurationCpy.end(), 0.0) / nanoDurationCpy.size();

    size_t middleCreate = nanoDurationCreate.size() / 2;
    size_t middleCpy = nanoDurationCpy.size() / 2;

    std::cout << "Creation average: " << averageCreate << " median: " << nanoDurationCreate[middleCreate];
    std::cout << " min: " << nanoDurationCreate[0] << " max: " << nanoDurationCreate[nanoDurationCreate.size() - 1] << std::endl;
    std::cout << "Copy average: " << averageCpy << " median: " << nanoDurationCpy[middleCpy];
    std::cout << " min: " << nanoDurationCpy[0] << " max: " << nanoDurationCpy[nanoDurationCpy.size() - 1] << std::endl;

    for (uint32_t i = 0; i < maxLoop; i++) {
        std::cout << "#" << (i + 1) << " Create: " << nanoDurationCreate[i] << " Copy: " << nanoDurationCpy[i] << std::endl;
    }

    for (uint32_t i = 0; i < maxLoop; ++i) {
        alignedFree(surfaceStates[i]);
        alignedFree(copyBuffers[i]);
    }
}
