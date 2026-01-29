#pragma once

#include "ui_ResyncConfig.h"

#include <QtWidgets/QDialog>

namespace resync
{

	class ResyncConfigDialog final : public QDialog
	{
		Q_OBJECT

	public:
		explicit ResyncConfigDialog(QWidget* parent = nullptr);
		~ResyncConfigDialog() override {}

		static QString get_config_base_path();
		static QString get_config_file_path();

		void accept() override { on_apply(); }
		void reject() override { on_cancel(); }

		void on_apply();
		void on_cancel();

		void on_change();
		void on_server_address_changed(const QString& text);
		void on_server_port_changed(const QString& text);

		void on_server_test_clicked();

	private:
		bool is_valid_server_address(const QString& address);
		bool is_valid_server_port(int port);
		bool validate_server_fields();

	private:
		Ui::ResyncConfigDialog m_ui;

		bool m_bHasChanged = false;

		QString m_serverAddress;
		int m_serverPort = 0;

	};

} // namespace resync
