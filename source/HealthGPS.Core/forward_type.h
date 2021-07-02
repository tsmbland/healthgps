#pragma once
#include <type_traits>

// forward type declaration
namespace hgps {
	namespace core {

		// C++20 concept for numeric columns types
		template <typename T>
		concept Numerical = std::is_arithmetic_v<T>;

		enum class Gender : uint8_t {
			unknown,
			male,
			female
		};

		class DataTable;
		class DataTableColumn;

		class StringDataTableColumn;
		class FloatDataTableColumn;
		class DoubleDataTableColumn;
		class IntegerDataTableColumn;

		class DataTableColumnVisitor;
	}
}