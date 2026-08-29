#pragma once

#include "UnrealVoxelSim/Events/Api/IPublisher.h"
#include "UnrealVoxelSim/Events/Api/ISource.h"
#include "UnrealVoxelSim/Voxel/Api/IEditor.h"
#include "UnrealVoxelSim/Voxel/Api/IReader.h"
#include "UnrealVoxelSim/Voxel/Api/IRegionReader.h"
#include "UnrealVoxelSim/Voxel/Solid/Api/Changed.h"
#include "UnrealVoxelSim/Voxel/Solid/Api/IChangeSource.h"
#include "UnrealVoxelSim/Voxel/Solid/Api/ICommands.h"
#include "UnrealVoxelSim/Voxel/Solid/Api/IReader.h"
#include "UnrealVoxelSim/Voxel/Solid/Api/IRegionReader.h"
#include "UnrealVoxelSim/Voxel/Solid/Api/MaterialId.h"

#include <memory>
#include <span>

namespace UnrealVoxelSim::Voxel::Solid
{
	class Controller final : public Api::IReader, public Api::IRegionReader, public Api::ICommands
	{
	public:
		Controller(UnrealVoxelSim::Voxel::Api::IReader& reader,
		           UnrealVoxelSim::Voxel::Api::IRegionReader& regionReader,
		           UnrealVoxelSim::Voxel::Api::IEditor& editor,
		           std::span<const Api::MaterialId> materials,
		           std::unique_ptr<Events::Api::ISource<Api::Changed>> changeSource,
		           Events::Api::IPublisher<Api::Changed>& changePublisher);
		~Controller() override;

		Controller(const Controller&) = delete;
		Controller& operator=(const Controller&) = delete;
		Controller(Controller&&) = delete;
		Controller& operator=(Controller&&) = delete;

		[[nodiscard]] std::expected<Api::Cell, UnrealVoxelSim::Voxel::Api::ReadError> Read(
			UnrealVoxelSim::Voxel::Api::Position position) const noexcept override;

		[[nodiscard]] std::expected<void, UnrealVoxelSim::Voxel::Api::ReadError> ReadRegion(
			UnrealVoxelSim::Voxel::Api::Region region,
			std::span<Api::Cell> output) const override;

		[[nodiscard]] std::expected<Api::EditResult, Api::EditFailure> Place(
			std::span<const Api::Placement> placements) override;

		[[nodiscard]] std::expected<Api::EditResult, Api::EditFailure> Remove(
			std::span<const UnrealVoxelSim::Voxel::Api::Position> positions) override;

		[[nodiscard]] Api::IChangeSource& Changes() noexcept;

	private:
		class Impl;
		std::unique_ptr<Impl> m_Impl;
	};
}
