#pragma once

#include "ccc/symbol_database.h"

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>


#ifdef _WIN32
#define RESYNC_API __declspec(dllexport)
#else
#define RESYNC_API [[gnu::visibility("default")]]
#endif


class QMainWindow;
class QAction;
class QMenu;

namespace resync
{
	using u8 = std::uint8_t;
	using u16 = std::uint16_t;
	using u32 = std::uint32_t;
	using u64 = std::uint64_t;

	template <typename T>
	concept PointerSizeType = std::same_as<T, u32>;

	template <PointerSizeType T = u32>
	class GenericAddress
	{
	public:
		GenericAddress() = default;
		GenericAddress(ccc::Address const& addr)
        {
			m_value = static_cast<T>(addr.value);
        }

		bool is_relative() const { return m_is_relative; }
		T value() const { return m_value; }

	private:
		bool m_is_relative{};
		T m_value{};
	};

	using Address = GenericAddress<u32>;

	class Function
	{
	public:
		Function(std::string_view name, Address address) : m_name(name), m_address(address) {}

		std::string name() const { return m_name; }
		Address const& address() const { return m_address; }

    private:
		std::string m_name;
		Address m_address;
	};

}

namespace resync::pcsx2
{
	// Internal state representation for PCSX2 symbols. This is an internal "interpretation," not the actual PCSX2 internal symbol data.
	class PCSX2Symbols
	{
	public:
		static PCSX2Symbols create_from_pcsx2_symbols(ccc::SymbolDatabase const& db);
	};

	class Function
	{
	public:
		Function(std::string_view name, Address address)
            : m_name(name), m_address(address) {}
	private:
		std::string m_name;
		Address m_address;
	};
}

namespace resync
{

	class RESYNC_API ResyncPlugin final : public QObject
	{
		Q_OBJECT

		using FunctionList = std::vector<Function>;

	public:
		ResyncPlugin(QWidget* parent) : QObject(parent) {}
		~ResyncPlugin() override {}

		void init(QMainWindow* main_window);

		void sync_pcsx2();
		void add_function(Function const& function);

		// TODO: Implement export based on dump_symbol_database
		void dump_symbol_database();
		void import_symbols_database(std::string_view filename);

		static void show_config_dialog();
		static void show_about_dialog();
		void test_command();

	private:
		QMainWindow* m_main_window = nullptr;

	private:
		FunctionList const& regen_function_list();

	private:
		FunctionList m_function_list;

	};
} // namespace resync
