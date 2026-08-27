#include "UnrealVoxelSim/Voxel/Solid/Controller.h"

#include "UnrealVoxelSim/Events/InMemory/Dispatcher.h"
#include "UnrealVoxelSim/Voxel/Chunked/Field.h"
#include "UnrealVoxelSim/Voxel/Solid/Api/StandardMaterials.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <vector>

namespace UnrealVoxelSim::Voxel::Solid
{
namespace
{

using Api::Cell;
using Api::Changed;
using Api::EditError;
using Api::Placement;
using UnrealVoxelSim::Voxel::Api::Position;
using UnrealVoxelSim::Voxel::Api::Region;

constexpr std::array Materials{
    Api::StandardMaterials::Dirt,
    Api::StandardMaterials::Grass,
    Api::StandardMaterials::Stone,
};

class ControllerTest : public ::testing::Test
{
  protected:
    static Controller CreateController(Events::InMemory::Dispatcher &dispatcher, Chunked::Field &field)
    {
        auto changes = dispatcher.CreateChannel<Changed>();
        auto &publisher = static_cast<Events::Api::IPublisher<Changed> &>(*changes);
        return Controller{field, field, field, Materials, std::move(changes), publisher};
    }

    ControllerTest()
        : Field(Region{{-64, -64, -64}, {64, 64, 64}}),
          Solids(CreateController(Dispatcher, Field))
    {
    }

    Events::InMemory::Dispatcher Dispatcher;
    Chunked::Field Field;
    Controller Solids;
};

TEST_F(ControllerTest, PlacesAndReadsTheThreeStandardMaterials)
{
    const std::array placements{
        Placement{{-1, 0, 0}, Api::StandardMaterials::Dirt},
        Placement{{0, 0, 0}, Api::StandardMaterials::Grass},
        Placement{{1, 0, 0}, Api::StandardMaterials::Stone},
    };

    const auto result = Solids.Place(placements);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ChangedVoxelCount, 3U);
    EXPECT_EQ(Solids.Read({-1, 0, 0})->Material(), Api::StandardMaterials::Dirt);
    EXPECT_EQ(Solids.Read({0, 0, 0})->Material(), Api::StandardMaterials::Grass);
    EXPECT_EQ(Solids.Read({1, 0, 0})->Material(), Api::StandardMaterials::Stone);
}

TEST_F(ControllerTest, RejectsUnknownMaterialsBeforeChangingStorage)
{
    const Placement placement{{0, 0, 0}, Api::MaterialId{99}};

    const auto result = Solids.Place(std::span{&placement, 1});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().Error, EditError::UnknownMaterial);
    EXPECT_TRUE(Solids.Read({0, 0, 0})->IsEmpty());
}

TEST_F(ControllerTest, PlacementBatchIsAtomicWhenOnePositionIsOccupied)
{
    const Placement initial{{1, 0, 0}, Api::StandardMaterials::Stone};
    ASSERT_TRUE(Solids.Place(std::span{&initial, 1}).has_value());
    const std::array placements{
        Placement{{0, 0, 0}, Api::StandardMaterials::Dirt},
        Placement{{1, 0, 0}, Api::StandardMaterials::Grass},
    };

    const auto result = Solids.Place(placements);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().Error, EditError::Occupied);
    EXPECT_EQ(result.error().InputIndex, 1U);
    EXPECT_TRUE(Solids.Read({0, 0, 0})->IsEmpty());
    EXPECT_EQ(Solids.Read({1, 0, 0})->Material(), Api::StandardMaterials::Stone);
}

TEST_F(ControllerTest, RemovesOccupiedVoxelsAndRejectsEmptyOnes)
{
    const Placement placement{{0, 0, 0}, Api::StandardMaterials::Grass};
    ASSERT_TRUE(Solids.Place(std::span{&placement, 1}).has_value());
    const Position position{0, 0, 0};

    ASSERT_TRUE(Solids.Remove(std::span{&position, 1}).has_value());
    EXPECT_TRUE(Solids.Read(position)->IsEmpty());

    const auto secondRemoval = Solids.Remove(std::span{&position, 1});
    ASSERT_FALSE(secondRemoval.has_value());
    EXPECT_EQ(secondRemoval.error().Error, EditError::Empty);
}

TEST_F(ControllerTest, ReadsLogicalRegionsWithoutExposingStoragePartitions)
{
    const std::array placements{
        Placement{{-1, 0, 0}, Api::StandardMaterials::Dirt},
        Placement{{0, 0, 0}, Api::StandardMaterials::Grass},
        Placement{{1, 0, 0}, Api::StandardMaterials::Stone},
    };
    ASSERT_TRUE(Solids.Place(placements).has_value());
    std::array<Cell, 3> output;

    ASSERT_TRUE(Solids.ReadRegion({{-1, 0, 0}, {2, 1, 1}}, output).has_value());
    EXPECT_EQ(output[0].Material(), Api::StandardMaterials::Dirt);
    EXPECT_EQ(output[1].Material(), Api::StandardMaterials::Grass);
    EXPECT_EQ(output[2].Material(), Api::StandardMaterials::Stone);
}

TEST_F(ControllerTest, PublishesOneQueuedEventWithCoalescedLogicalRuns)
{
    std::size_t deliveryCount = 0;
    std::vector<Region> deliveredRegions;
    auto subscription = Solids.Changes().Subscribe([&](const Changed &event) noexcept {
        ++deliveryCount;
        deliveredRegions = event.Regions;
    });
    const std::array placements{
        Placement{{1, 2, 3}, Api::StandardMaterials::Dirt},
        Placement{{2, 2, 3}, Api::StandardMaterials::Grass},
        Placement{{10, 2, 3}, Api::StandardMaterials::Stone},
    };

    ASSERT_TRUE(Solids.Place(placements).has_value());
    EXPECT_EQ(deliveryCount, 0U);
    ASSERT_TRUE(Dispatcher.DispatchPending().has_value());

    EXPECT_EQ(deliveryCount, 1U);
    ASSERT_EQ(deliveredRegions.size(), 2U);
    EXPECT_EQ(deliveredRegions[0], (Region{{1, 2, 3}, {3, 3, 4}}));
    EXPECT_EQ(deliveredRegions[1], (Region{{10, 2, 3}, {11, 3, 4}}));
}

TEST_F(ControllerTest, FailedCommandsDoNotPublishEvents)
{
    std::size_t deliveryCount = 0;
    auto subscription = Solids.Changes().Subscribe([&](const Changed &) noexcept { ++deliveryCount; });
    const Placement invalid{{0, 0, 0}, Api::MaterialId{99}};

    ASSERT_FALSE(Solids.Place(std::span{&invalid, 1}).has_value());
    EXPECT_FALSE(Dispatcher.HasPending());
    EXPECT_EQ(deliveryCount, 0U);
}

}
}
