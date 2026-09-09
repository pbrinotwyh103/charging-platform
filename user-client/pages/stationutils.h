#pragma once
#include <QJsonArray>
namespace StationUtils {
bool isValidCoordinate(double latitude, double longitude);
double distanceKm(double lat1, double lon1, double lat2, double lon2);
QJsonArray sortByDistance(const QJsonArray &stations, double latitude, double longitude);
}
