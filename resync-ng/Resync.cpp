//
// Created by mark on 1/26/26.
//

#include <nlohmann/json.hpp>

#include "Resync.h"

#include "ResyncConfigDialog.h"
#include "ResyncAboutDialog.h"

#include "DebugTools/SymbolGuardian.h"
#include "Debugger/Docking/DockMenuBar.h"


#include <QFileDialog>
#include <QMessageBox>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <random>
#include <vector>


#define RESYNC_INTERNAL static


namespace resync
{

	RESYNC_INTERNAL void dbg(const std::string& msg)
	{
		std::cout << "[Debug] Resync: " << msg << std::endl;
	}

	namespace fs = std::filesystem;

	RESYNC_INTERNAL void open_with_default_app(const fs::path& file)
	{
#if defined(_WIN32)

		// Windows: ShellExecuteW with verb "open".[web:106][web:107][web:115]
		std::wstring wpath = file.wstring();
		HINSTANCE res = ShellExecuteW(
			nullptr,
			L"open",
			wpath.c_str(),
			nullptr,
			nullptr,
			SW_SHOWNORMAL
		);
		// Optionally check res <= (HINSTANCE)32 for errors.

#elif defined(__APPLE__)

		// macOS: `open` uses LaunchServices and the default association.[web:110][web:116]
		std::string cmd = "open \"" + file.string() + "\"";
		std::system(cmd.c_str());

#else

		// Linux / BSD: xdg-open defers to the desktop’s default handler.[web:111][web:117]
		std::string cmd = "xdg-open \"" + file.string() + "\"";
		std::system(cmd.c_str());

#endif
	}

	RESYNC_INTERNAL fs::path make_temp_text_file(std::string_view contents)
	{
		std::random_device random_device;
		std::mt19937 generator(random_device());
		std::uniform_int_distribution<u32> dist(0, UINT32_MAX);
		u32 rnd = dist(generator);

		fs::path dir = fs::temp_directory_path();
		fs::path file = dir / fs::path(std::format("tmp_{:08X}.txt", rnd));

		std::ofstream out(file);
		out << contents;
		return file;
	}

	pcsx2::PCSX2Symbols pcsx2::PCSX2Symbols::create_from_pcsx2_symbols(ccc::SymbolDatabase const& db)
	{
	    // For now, just return an empty symbol set.
	    pcsx2::PCSX2Symbols symbols;
	    return symbols;

	}

	void ResyncPlugin::init(QMainWindow* main_window)
	{
		resync::dbg("Initializing resync-ng plugin...");

		m_main_window = main_window;

		QMenu* resync_menu = main_window->menuBar()->addMenu("&resync-ng");
		QAction* dump_symbols_action = resync_menu->addAction("&Dump symbols");
		QObject::connect(dump_symbols_action, &QAction::triggered, this, [this]() {
            dump_symbol_database();
        });
		QAction* import_symbols_action = resync_menu->addAction("&Import symbols...");
		QObject::connect(import_symbols_action, &QAction::triggered, this, [this]() {
            QString filename = QFileDialog::getOpenFileName(
                m_main_window,
                tr("Import Symbols Database"),
                "",
                tr("Resync Symbols Database (*.json);;All Files (*)")
            );
            if (!filename.isEmpty()) {
                import_symbols_database(filename.toStdString());
            }
        });
		QAction* test_action = resync_menu->addAction("&Test");
		QObject::connect(test_action, &QAction::triggered, this, [this]() {
			test_command();
		});
		QAction* config_action = resync_menu->addAction("&Configuration...");
		QObject::connect(config_action, &QAction::triggered, this, []() {
			show_config_dialog();
		});
		resync_menu->addSeparator();
		QAction* about_action = resync_menu->addAction("&About resync-ng");
		QObject::connect(about_action, &QAction::triggered, this, []() {
			show_about_dialog();
		});
	}

	void ResyncPlugin::add_function(Function const& fn)
	{
		m_function_list.push_back(std::move(fn));
	}

	void ResyncPlugin::sync_pcsx2()
	{
		resync::dbg("Syncing functions with PCSX2 symbol database...");

			for (auto const& fn : m_function_list)
			{
				// Does it exist? If so, rename it
				FunctionInfo const existing_fn = R5900SymbolGuardian.FunctionStartingAtAddress(fn.address().value());
				if (existing_fn.address.valid())
				{
					R5900SymbolGuardian.ReadWrite([&](ccc::SymbolDatabase& db) {
						db.functions.rename_symbol(existing_fn.handle, fn.name());
					});
				}
				// Else, create it
				else
				{
					R5900SymbolGuardian.ReadWrite([&](ccc::SymbolDatabase& db) {
						if (auto result = db.functions.create_symbol(
                                fn.name(),
                                ccc::Address(fn.address().value()),
                                // Use "User-Defined" as the symbol source for now
                                *db.get_symbol_source("User-Defined"),
                                nullptr
                            );
                            !result.success())
                        {
                            auto msg = std::format("Failed to add function {} at address {:08X}", fn.name(), fn.address().value());
							resync::dbg(msg);
							QMessageBox::warning(
								m_main_window,
								tr("Cannot Create Function"),
								tr(msg.c_str())
                            );
                        }
						else
						{
							resync::dbg(std::format("Added function {} at address {:08X} to PCSX2 symbol database.", fn.name(), fn.address().value()));
						}
					});
				}
			}
	}

	void ResyncPlugin::show_config_dialog()
	{
		ResyncConfigDialog dialog;
		dialog.exec();
	}

	void ResyncPlugin::show_about_dialog()
	{
		ResyncAboutDialog dialog;
		dialog.exec();
	}

    void ResyncPlugin::dump_symbol_database() {
		resync::dbg("Dumping symbol database to file...");
	    regen_function_list();

	    namespace fs = std::filesystem;
	    fs::path dump_path = fs::temp_directory_path() / "resync_symbol_dump.json";
		resync::dbg(std::format("Dump filename: {}", dump_path.string()));

		using JSON = nlohmann::json;
		JSON json;

		// Dump functions
		json["functions"] = JSON::array();
		for (Function const& fn : m_function_list) {
            JSON fn_json;
            fn_json["name"] = fn.name();
            fn_json["address"] = fn.address().value();
            json["functions"].push_back(fn_json);
        }

		std::ofstream dump_file(dump_path);
		dump_file << json.dump(2);
		dump_file.close();

		open_with_default_app(dump_path.string());
	}

	// Import an existing symbols database from a file previously dumped in resync format.
	void ResyncPlugin::import_symbols_database(std::string_view filename)
	{
		// WARNING: This will blindly add functions to the current symbol database without checking for
		//	duplicates and it WILL OVERWRITE EXISTING functions. Use with caution.

		auto import_path = fs::path(filename);

		if (!fs::exists(import_path)) {
            QMessageBox::warning(
                m_main_window,
                tr("Import Symbols Database"),
                tr("The specified symbols database file does not exist.")
            );
            return;
        }

		std::ifstream import_file(import_path);
		using JSON = nlohmann::json;
		JSON json;

		json = JSON::parse(import_file);
		for (const auto& fn_json : json["functions"])
		{
			std::string name = fn_json["name"];
			u32 address = fn_json["address"];
			add_function(Function(name, Address(address)));
		}

		sync_pcsx2();

	}

	void ResyncPlugin::test_command()
	{
		resync::dbg("Running test command...");
        R5900SymbolGuardian.ReadWrite([&](ccc::SymbolDatabase& database) {
			// Symbol database:
            //
            // class SymbolDatabase {
            // public:
            // 	SymbolList<DataType> data_types;
            // 	SymbolList<Function> functions;
            // 	SymbolList<GlobalVariable> global_variables;
            // 	SymbolList<Label> labels;
            // 	SymbolList<LocalVariable> local_variables;
            // 	SymbolList<Module> modules;
            // 	SymbolList<ParameterVariable> parameter_variables;
            // 	SymbolList<Section> sections;
            // 	SymbolList<SourceFile> source_files;
            // 	SymbolList<SymbolSource> symbol_sources;

            // Attempt to add function
            dbg("Attempting to add function to database...");
        	ccc::Result<ccc::SymbolSourceHandle> source = database.get_symbol_source("User-Defined");
        	if (!source.success())
        	{
        		QMessageBox::warning(
        			m_main_window,
        			tr("Cannot Create Function"),
        			tr("Cannot create symbol source.")
                );
                return;
        	}

        	ccc::Result<ccc::Function*> function = database.functions.create_symbol(
        		"DoDamage",
        		ccc::Address(0x003E2130),
        		*source,
        		nullptr
            );
        	if (!function.success())
        	{
        		QMessageBox::warning(
                    m_main_window,
                    tr("Cannot Create Function"),
                    tr("Cannot create symbol source.")
                );
        		return;
        	}


            dbg("R5900 Symbol Database has been read...");

			// What all goodies does it have?
			std::vector<std::string> output;

			output.push_back("\n");
			output.push_back("==========================================");
			output.push_back("\n");
			output.push_back("SymbolDatabase includes the following:\n");
            output.push_back("  - SymbolList<DataType> data_types;");
            output.push_back("  - SymbolList<Function> functions;");
            output.push_back("  - SymbolList<GlobalVariable> global_variables;");
            output.push_back("  - SymbolList<Label> labels;");
            output.push_back("  - SymbolList<LocalVariable> local_variables;");
            output.push_back("  - SymbolList<Module> modules;");
            output.push_back("  - SymbolList<ParameterVariable> parameter_variables;");
            output.push_back("  - SymbolList<Section> sections;");
            output.push_back("  - SymbolList<SourceFile> source_files;");
            output.push_back("  - SymbolList<SymbolSource> symbol_sources;");


			output.push_back("\n");
			output.push_back("==========================================");
			output.push_back("\n");
			output.push_back(std::format("Total symbols: {}", database.symbol_count()));
			output.push_back(std::format("Total DataTypes: {}", database.data_types.size()));
			output.push_back(std::format("Total Functions: {}", database.functions.size()));
			output.push_back(std::format("Total GlobalVariables: {}", database.global_variables.size()));
			output.push_back(std::format("Total Labels: {}", database.labels.size()));
			output.push_back(std::format("Total LocalVariables: {}", database.local_variables.size()));
			output.push_back(std::format("Total Modules: {}", database.modules.size()));
			output.push_back(std::format("Total ParameterVariables: {}", database.parameter_variables.size()));
			output.push_back(std::format("Total Sections: {}", database.sections.size()));
			output.push_back(std::format("Total SourceFiles: {}", database.source_files.size()));
			output.push_back(std::format("Total SymbolSources: {}", database.symbol_sources.size()));

			output.push_back("\n");
			output.push_back("==========================================");
			output.push_back("\n");
			output.push_back("DataTypes:");
			for (const auto& dt : database.data_types)
			{
				output.push_back(std::format(" - {}", dt.name()));
			}

			output.push_back("\n");
			output.push_back("==========================================");
			output.push_back("\n");
			output.push_back("Functions:");
			for (const auto& fn : database.functions)
            {
				u32 mod = fn.module_handle().value;
				u32 offset = fn.address().get_or_zero();
                output.push_back(std::format(" - Mod:{:08x} : {:08x} : {}", mod, offset, fn.name()));
            }

			output.push_back("\n");
			output.push_back("=========================================");
			output.push_back("\n");
			output.push_back("GlobalVariables:");
			for (const auto& gv : database.global_variables)
            {
                output.push_back(std::format(" - {}", gv.name()));
            }

			output.push_back("\n");
			output.push_back("=========================================");
			output.push_back("\n");
			output.push_back("Labels:");
			for (const auto& lbl : database.labels)
            {
                output.push_back(std::format(" - {}", lbl.name()));
            }

			output.push_back("\n");
			output.push_back("=========================================");
			output.push_back("\n");
			output.push_back("LocalVariables:");
			for (const auto& lv : database.local_variables)
            {
                output.push_back(std::format(" - {}", lv.name()));
            }

			output.push_back("\n");
			output.push_back("=========================================");
			output.push_back("\n");
			output.push_back("Modules:");
			for (const auto& mod : database.modules)
            {
                output.push_back(std::format(" - {}", mod.name()));
            }

			output.push_back("\n");
			output.push_back("=========================================");
			output.push_back("\n");
			output.push_back("ParameterVariables:");
			for (const auto& pv : database.parameter_variables)
            {
                output.push_back(std::format(" - {}", pv.name()));
            }

			output.push_back("\n");
			output.push_back("=========================================");
			output.push_back("\n");
			output.push_back("Sections:");
			for (const auto& sec : database.sections)
            {
                output.push_back(std::format(" - {}", sec.name()));
            }

			output.push_back("\n");
			output.push_back("=========================================");
			output.push_back("\n");
			output.push_back("SourceFiles:");
			for (const auto& sf : database.source_files)
            {
                output.push_back(std::format(" - {}", sf.name()));
            }

			output.push_back("\n");
			output.push_back("=========================================");
			output.push_back("\n");
			output.push_back("SymbolSources:");
			for (const auto& ss : database.symbol_sources)
            {
                output.push_back(std::format(" - {}", ss.name()));
            }

			auto output_str = std::accumulate(
                std::next(output.begin()), output.end(), output[0],
                [](std::string a, std::string b) { return a + "\n" + b; }
            );

			auto temp_file = make_temp_text_file(output_str);
			open_with_default_app(temp_file);

		});
	}

	ResyncPlugin::FunctionList const& ResyncPlugin::regen_function_list()
	{
		R5900SymbolGuardian.Read([&](ccc::SymbolDatabase const& db) {
            resync::dbg("Regenerating function list from R5900 symbol database...");
			m_function_list.clear();
            for (ccc::Function const& fn : db.functions)
            {
            	if (fn.name().starts_with("z_un_")) continue;

	            m_function_list.push_back(resync::Function(fn.name(), resync::Address(fn.address().value)));
            }
		});

		return m_function_list;
	}

} // namespace resync