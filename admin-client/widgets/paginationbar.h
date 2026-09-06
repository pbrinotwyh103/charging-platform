#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

class PaginationBar final : public QWidget {
  Q_OBJECT

public:
  explicit PaginationBar(QWidget *parent = nullptr);

  int currentPage() const;
  int totalPages() const;
  void setPage(int currentPage, int totalPages, int totalItems = -1);

signals:
  void pageRequested(int page);

private:
  QLabel *m_pageLabel = nullptr;
  QPushButton *m_previousButton = nullptr;
  QPushButton *m_nextButton = nullptr;
  int m_currentPage = 1;
  int m_totalPages = 1;
  int m_totalItems = -1;
};
