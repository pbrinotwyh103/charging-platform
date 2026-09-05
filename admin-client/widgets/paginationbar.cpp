#include "widgets/paginationbar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

PaginationBar::PaginationBar(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("paginationBar"));
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  m_previousButton = new QPushButton(this);
  m_previousButton->setObjectName(QStringLiteral("previousPageButton"));
  m_previousButton->setIcon(style()->standardIcon(QStyle::SP_ArrowLeft));
  m_previousButton->setToolTip(QStringLiteral("上一页"));
  m_previousButton->setAccessibleName(QStringLiteral("上一页"));
  m_previousButton->setFixedSize(44, 44);
  m_pageLabel = new QLabel(this);
  m_pageLabel->setObjectName(QStringLiteral("pageNumberLabel"));
  m_pageLabel->setAlignment(Qt::AlignCenter);
  m_pageLabel->setMinimumWidth(96);
  m_nextButton = new QPushButton(this);
  m_nextButton->setObjectName(QStringLiteral("nextPageButton"));
  m_nextButton->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
  m_nextButton->setToolTip(QStringLiteral("下一页"));
  m_nextButton->setAccessibleName(QStringLiteral("下一页"));
  m_nextButton->setFixedSize(44, 44);
  layout->addStretch();
  layout->addWidget(m_previousButton);
  layout->addWidget(m_pageLabel);
  layout->addWidget(m_nextButton);

  connect(m_previousButton, &QPushButton::clicked, this, [this] {
    if (m_currentPage > 1)
      emit pageRequested(m_currentPage - 1);
  });
  connect(m_nextButton, &QPushButton::clicked, this, [this] {
    if (m_currentPage < m_totalPages)
      emit pageRequested(m_currentPage + 1);
  });
  setPage(1, 1);
}

int PaginationBar::currentPage() const { return m_currentPage; }

int PaginationBar::totalPages() const { return m_totalPages; }

void PaginationBar::setPage(int currentPage, int totalPages, int totalItems) {
  m_totalPages = qMax(1, totalPages);
  m_currentPage = qBound(1, currentPage, m_totalPages);
  m_totalItems = totalItems;
  m_previousButton->setEnabled(m_currentPage > 1);
  m_nextButton->setEnabled(m_currentPage < m_totalPages);
  m_pageLabel->setText(
      m_totalItems >= 0
          ? QStringLiteral("%1 / %2 · %3条")
                .arg(m_currentPage)
                .arg(m_totalPages)
                .arg(m_totalItems)
          : QStringLiteral("%1 / %2").arg(m_currentPage).arg(m_totalPages));
}
