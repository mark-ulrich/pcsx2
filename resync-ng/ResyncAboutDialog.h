#pragma once
#include "ui_ResyncAboutDialog.h"
#include <QDialog>

namespace resync
{

	class ResyncAboutDialog final : public QDialog
	{
		Q_OBJECT

		public:
		explicit ResyncAboutDialog(QWidget* parent = nullptr);
		~ResyncAboutDialog() override {}

	private:
		Ui::ResyncAboutDialog m_ui;

	};

} // namespace resync
