#include "UnrealVoxelSim/Voxel/Solid/Controller.h"

#include "UnrealVoxelSim/Voxel/Api/CellMutation.h"
#include "UnrealVoxelSim/Voxel/Api/EditError.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace UnrealVoxelSim::Voxel::Solid
{
	namespace
	{
		[[nodiscard]] Api::Cell ToCell(const UnrealVoxelSim::Voxel::Api::CellValue value) noexcept
		{
			return Api::Cell{Api::MaterialId{value.Value()}};
		}

		[[nodiscard]] UnrealVoxelSim::Voxel::Api::CellValue ToCellValue(const Api::MaterialId material) noexcept
		{
			return UnrealVoxelSim::Voxel::Api::CellValue{material.Value()};
		}

		[[nodiscard]] std::vector<UnrealVoxelSim::Voxel::Api::Region> MakeChangedRegions(
			std::vector<UnrealVoxelSim::Voxel::Api::Position> positions)
		{
			std::sort(positions.begin(), positions.end(), [](const auto left, const auto right)
			{
				if (left.Z != right.Z)
				{
					return left.Z < right.Z;
				}
				if (left.Y != right.Y)
				{
					return left.Y < right.Y;
				}
				return left.X < right.X;
			});

			std::vector<UnrealVoxelSim::Voxel::Api::Region> regions;
			regions.reserve(positions.size());
			for (const auto position : positions)
			{
				if (!regions.empty() && regions.back().Min.Y == position.Y && regions.back().Min.Z == position.Z &&
					regions.back().Max.X == position.X)
				{
					++regions.back().Max.X;
					continue;
				}
				regions.push_back({position, {position.X + 1, position.Y + 1, position.Z + 1}});
			}
			return regions;
		}
	}

	class Controller::Impl final
	{
	public:
		Impl(UnrealVoxelSim::Voxel::Api::IReader& reader,
		     UnrealVoxelSim::Voxel::Api::IRegionReader& regionReader,
		     UnrealVoxelSim::Voxel::Api::IEditor& editor,
		     std::span<const Api::MaterialId> materials,
		     std::unique_ptr<Events::Api::ISource<Api::Changed>> changeSource,
		     Events::Api::IPublisher<Api::Changed>& changePublisher)
			: Reader(reader),
			  RegionReader(regionReader),
			  Editor(editor),
			  Materials(materials.begin(), materials.end()),
			  ChangeSource(std::move(changeSource)),
			  ChangePublisher(changePublisher)
		{
			if (!ChangeSource)
			{
				throw std::invalid_argument{"A solid voxel change channel is required."};
			}
			if (std::ranges::any_of(Materials, [](const Api::MaterialId material) { return !material.IsValid(); }))
			{
				throw std::invalid_argument{"Configured solid material identifiers must be valid."};
			}
			std::ranges::sort(Materials);
			if (std::ranges::adjacent_find(Materials) != Materials.end())
			{
				throw std::invalid_argument{"Configured solid material identifiers must be unique."};
			}
		}

		void AssertOwnerThread() const noexcept
		{
			assert(std::this_thread::get_id() == OwnerThread);
		}

		[[nodiscard]] bool IsKnown(const Api::MaterialId material) const noexcept
		{
			return std::ranges::binary_search(Materials, material);
		}

		void Publish(std::vector<UnrealVoxelSim::Voxel::Api::Position> positions)
		{
			if (!positions.empty())
			{
				ChangePublisher.Publish(Api::Changed{MakeChangedRegions(std::move(positions))});
			}
		}

		UnrealVoxelSim::Voxel::Api::IReader& Reader;
		UnrealVoxelSim::Voxel::Api::IRegionReader& RegionReader;
		UnrealVoxelSim::Voxel::Api::IEditor& Editor;
		std::vector<Api::MaterialId> Materials;
		std::unique_ptr<Events::Api::ISource<Api::Changed>> ChangeSource;
		Events::Api::IPublisher<Api::Changed>& ChangePublisher;
		mutable std::vector<UnrealVoxelSim::Voxel::Api::CellValue> RegionScratch;
		std::thread::id OwnerThread{std::this_thread::get_id()};
	};

	Controller::Controller(UnrealVoxelSim::Voxel::Api::IReader& reader,
	                       UnrealVoxelSim::Voxel::Api::IRegionReader& regionReader,
	                       UnrealVoxelSim::Voxel::Api::IEditor& editor,
	                       const std::span<const Api::MaterialId> materials,
	                       std::unique_ptr<Events::Api::ISource<Api::Changed>> changeSource,
	                       Events::Api::IPublisher<Api::Changed>& changePublisher)
		: m_Impl(
			std::make_unique<Impl>(reader, regionReader, editor, materials, std::move(changeSource), changePublisher))
	{
	}

	Controller::~Controller() = default;

	std::expected<Api::Cell, UnrealVoxelSim::Voxel::Api::ReadError> Controller::Read(
		const UnrealVoxelSim::Voxel::Api::Position position) const noexcept
	{
		m_Impl->AssertOwnerThread();
		const auto value = m_Impl->Reader.Read(position);
		if (!value)
		{
			return std::unexpected{value.error()};
		}
		return ToCell(*value);
	}

	std::expected<void, UnrealVoxelSim::Voxel::Api::ReadError> Controller::ReadRegion(
		const UnrealVoxelSim::Voxel::Api::Region region,
		const std::span<Api::Cell> output) const
	{
		m_Impl->AssertOwnerThread();
		m_Impl->RegionScratch.resize(output.size());
		const auto result = m_Impl->RegionReader.ReadRegion(region, m_Impl->RegionScratch);
		if (!result)
		{
			return std::unexpected{result.error()};
		}
		std::ranges::transform(m_Impl->RegionScratch, output.begin(), ToCell);
		return {};
	}

	std::expected<Api::EditResult, Api::EditFailure> Controller::Place(const std::span<const Api::Placement> placements)
	{
		m_Impl->AssertOwnerThread();
		std::vector<UnrealVoxelSim::Voxel::Api::CellMutation> mutations;
		std::vector<UnrealVoxelSim::Voxel::Api::Position> changedPositions;
		mutations.reserve(placements.size());
		changedPositions.reserve(placements.size());
		for (std::size_t index = 0; index < placements.size(); ++index)
		{
			if (!m_Impl->IsKnown(placements[index].Material))
			{
				return std::unexpected{Api::EditFailure{Api::EditError::UnknownMaterial, index, Api::Cell{}}};
			}
			mutations.push_back({placements[index].Position, {}, ToCellValue(placements[index].Material)});
			changedPositions.push_back(placements[index].Position);
		}

		const auto result = m_Impl->Editor.Apply(mutations);
		if (!result)
		{
			switch (result.error().Error)
			{
			case UnrealVoxelSim::Voxel::Api::EditError::OutOfBounds:
				return std::unexpected{
					Api::EditFailure{
						Api::EditError::OutOfBounds,
						result.error().MutationIndex,
						ToCell(result.error().Actual)
					}
				};
			case UnrealVoxelSim::Voxel::Api::EditError::DuplicatePosition:
				return std::unexpected{
					Api::EditFailure{
						Api::EditError::DuplicatePosition,
						result.error().MutationIndex,
						ToCell(result.error().Actual)
					}
				};
			case UnrealVoxelSim::Voxel::Api::EditError::ValueConflict:
				return std::unexpected{
					Api::EditFailure{
						result.error().Actual.IsEmpty()
							? Api::EditError::StorageConflict
							: Api::EditError::Occupied,
						result.error().MutationIndex,
						ToCell(result.error().Actual)
					}
				};
			}
			return std::unexpected{
				Api::EditFailure{
					Api::EditError::StorageConflict,
					result.error().MutationIndex,
					ToCell(result.error().Actual)
				}
			};
		}

		m_Impl->Publish(std::move(changedPositions));
		return Api::EditResult{result->ChangedCellCount};
	}

	std::expected<Api::EditResult, Api::EditFailure> Controller::Remove(
		const std::span<const UnrealVoxelSim::Voxel::Api::Position> positions)
	{
		m_Impl->AssertOwnerThread();
		std::vector<UnrealVoxelSim::Voxel::Api::CellMutation> mutations;
		mutations.reserve(positions.size());
		for (std::size_t index = 0; index < positions.size(); ++index)
		{
			const auto current = m_Impl->Reader.Read(positions[index]);
			if (!current)
			{
				return std::unexpected{Api::EditFailure{Api::EditError::OutOfBounds, index, Api::Cell{}}};
			}
			if (current->IsEmpty())
			{
				return std::unexpected{Api::EditFailure{Api::EditError::Empty, index, Api::Cell{}}};
			}
			mutations.push_back({positions[index], *current, {}});
		}

		const auto result = m_Impl->Editor.Apply(mutations);
		if (!result)
		{
			switch (result.error().Error)
			{
			case UnrealVoxelSim::Voxel::Api::EditError::OutOfBounds:
				return std::unexpected{
					Api::EditFailure{
						Api::EditError::OutOfBounds,
						result.error().MutationIndex,
						ToCell(result.error().Actual)
					}
				};
			case UnrealVoxelSim::Voxel::Api::EditError::DuplicatePosition:
				return std::unexpected{
					Api::EditFailure{
						Api::EditError::DuplicatePosition,
						result.error().MutationIndex,
						ToCell(result.error().Actual)
					}
				};
			case UnrealVoxelSim::Voxel::Api::EditError::ValueConflict:
				return std::unexpected{
					Api::EditFailure{
						Api::EditError::StorageConflict,
						result.error().MutationIndex,
						ToCell(result.error().Actual)
					}
				};
			}
			return std::unexpected{
				Api::EditFailure{
					Api::EditError::StorageConflict,
					result.error().MutationIndex,
					ToCell(result.error().Actual)
				}
			};
		}

		m_Impl->Publish({positions.begin(), positions.end()});
		return Api::EditResult{result->ChangedCellCount};
	}

	Api::IChangeSource& Controller::Changes() noexcept
	{
		m_Impl->AssertOwnerThread();
		return *m_Impl->ChangeSource;
	}
}
