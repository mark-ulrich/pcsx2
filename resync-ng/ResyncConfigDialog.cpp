//
// Created by mark on 1/26/26.
//

#include "ResyncConfigDialog.h"

#include <QDir>
#include <QEventLoop>
#include <QHostAddress>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRegularExpression>
#include <iostream>
#include <qstandardpaths.h>


namespace resync
{
	QString ResyncConfigDialog::get_config_base_path()
	{
		// Get local user application config path based on platform
		// If on Windows, use %APPDATA%\Resync\config.ini
		// If on macOS, use ~/Library/Application Support/Resync/config.ini
		// If on Linux, use ~/.config/resync/config.ini
		QString config_path;
#ifdef _WIN32
		config_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
						  + QDir::separator() + "config.ini";
#elif __APPLE__
		config_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
						  + QDir::separator() + "config.ini";
#else
		config_path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
						  + QDir::separator() + "resync" + QDir::separator() + "config.ini";
#endif
		return config_path;
	}

	QString ResyncConfigDialog::get_config_file_path()
	{
		const QString filename = "config.ini";
		return get_config_base_path() + QDir::separator() + filename;
	}

	ResyncConfigDialog::ResyncConfigDialog(QWidget* parent)
		: QDialog(parent)
	{
		// Init UI
		m_ui.setupUi(this);

		// Connect signals
		connect(m_ui.lineEditServerAddress, &QLineEdit::textChanged, this, &ResyncConfigDialog::on_server_address_changed);
		connect(m_ui.lineEditServerPort, &QLineEdit::textChanged, this, &ResyncConfigDialog::on_server_port_changed);
		connect(m_ui.btnTestServer, &QPushButton::clicked, this, &ResyncConfigDialog::on_server_test_clicked);

		connect(m_ui.buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &ResyncConfigDialog::on_apply);

		// Load existing config
		auto config_path = ResyncConfigDialog::get_config_base_path();
		QFile config_file(config_path);
		if (config_file.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			QTextStream in(&config_file);
			while (!in.atEnd())
			{
				QString line = in.readLine();
				if (line.startsWith("server_address="))
				{
					m_serverAddress = line.mid(QString("server_address=").length());
					m_ui.lineEditServerAddress->setText(m_serverAddress);
				}
				else if (line.startsWith("server_port="))
				{
					m_serverPort = line.mid(QString("server_port=").length()).toInt();
					m_ui.lineEditServerPort->setText(QString::number(m_serverPort));
				}
			}
		}

		m_ui.lineEditServerAddress->setText(m_serverAddress);
		m_ui.lineEditServerPort->setText(QString::number(m_serverPort));

		m_bHasChanged = false;
	}

	void
	ResyncConfigDialog::on_apply()
	{
		if (!validate_server_fields())
		{
			QMessageBox::critical(
				this,
				tr("Error"),
				tr("Invalid server address or port."));
			return;
		}

		auto config_path = ResyncConfigDialog::get_config_base_path();
		QFile config_file(config_path);
		if (!config_file.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			QMessageBox::critical(
				this,
				tr("Error"),
				tr("Failed to open config file for writing: %1").arg(config_path));
			return;
		}
		QTextStream out(&config_file);
		out << "server_address=" << m_serverAddress << "\n";
		out << "server_port=" << m_serverPort << "\n";
		config_file.close();
		m_bHasChanged = false;
		QDialog::accept();
	}

	void ResyncConfigDialog::on_cancel()
	{
		if (!m_bHasChanged)
		{
			return QDialog::reject();
		}

		auto areYouSure = QMessageBox::question(
			this,
			tr("Discard Changes"),
			tr("You have unsaved changes. Are you sure you want to discard them?"),
			QMessageBox::Yes | QMessageBox::No);
		if (areYouSure == QMessageBox::Yes)
		{
			return QDialog::reject();
		}
	}

	void ResyncConfigDialog::on_change()
	{
		m_bHasChanged = true;
	}

	void ResyncConfigDialog::on_server_address_changed(const QString& text)
	{
		m_serverAddress = text;
		on_change();
	}

	void ResyncConfigDialog::on_server_port_changed(const QString& text)
	{
		m_serverPort = text.toInt();
		on_change();
	}

	void ResyncConfigDialog::on_server_test_clicked()
	{
		if (!validate_server_fields())
		{
			QMessageBox::critical(
				this,
				tr("Error"),
				tr("Invalid server address or port."));
			return;
		}

		// Send an API GET request to server endpoint '/health-status' to test connection
		// copilot: send a GET request to the server
		QNetworkAccessManager manager(this);
		QUrl url(QString("http://%1:%2/health-status").arg(m_serverAddress).arg(m_serverPort));
		QNetworkRequest request(url);
		QNetworkReply* reply = manager.get(request);
		std::cout << "Sending request to " << url.toString().toStdString() << std::endl;
		QEventLoop loop;
		connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
		loop.exec();
		if (reply->error() != QNetworkReply::NoError)
		{
			QMessageBox::critical(
				this,
				tr("Error"),
				tr("Server does not respond: %1").arg(reply->errorString()));
			reply->deleteLater();
			return;
		}
		QMessageBox::information(
			this,
			tr("Success"),
			tr("Server is alive"));
		reply->deleteLater();



	}


	// ===========================================================
	//					Private Methods
	// ===========================================================
	bool ResyncConfigDialog::is_valid_server_address(const QString& address)
	{
		// Is valid IP?
		QHostAddress hostAddress(address);
		if (!hostAddress.isNull())
		{
			return true;
		}

		// Is valid hostname?
		QRegularExpression hostname_regex(R"(^(([a-zA-Z]{1})|([a-zA-Z]{1}[a-zA-Z]{1})|([a-zA-Z]{1}[0-9]{1})|([0-9]{1}[a-zA-Z]{1})|([a-zA-Z0-9]+[.-]{1}[a-zA-Z0-9]+)+)([a-zA-Z0-9.-]{0,61})([a-zA-Z0-9]{1,})\.?([a-zA-Z]{2,})$)");
		QRegularExpressionMatch match = hostname_regex.match(address);
		if (match.hasMatch())
		{
			return true;
		}

		// Default: invalid
		return false;
	}

	bool ResyncConfigDialog::is_valid_server_port(int port)
	{
		return port > 0 && port <= 65535;
	}

	bool ResyncConfigDialog::validate_server_fields()
	{
		return is_valid_server_address(m_serverAddress) && is_valid_server_port(m_serverPort);
	}



} // namespace resync