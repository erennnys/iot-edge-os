// Standalone Sunrise/Sunset calculations for Istanbul (Latitude: 41.0082, Longitude: 28.9784)
// Port of the standard NOAA Solar Calculator algorithm.

function getSolarTimes(date = new Date(), latitude = 41.0082, longitude = 28.9784) {
  const year = date.getFullYear();
  const month = date.getMonth() + 1;
  const day = date.getDate();
  
  // 1. Calculate Day of the Year
  const N1 = Math.floor(275 * month / 9);
  const N2 = Math.floor((month + 9) / 12);
  const N3 = (1 + Math.floor((year - 4 * Math.floor(year / 4) + 2) / 3));
  const N = N1 - (N2 * N3) + day - 30;

  // 2. Convert longitude to hour value and calculate approximate time
  const lngHour = longitude / 15.0;
  
  // Rise/Set approximate times
  const t_rise = N + ((6.0 - lngHour) / 24.0);
  const t_set = N + ((18.0 - lngHour) / 24.0);

  // 3. Calculate Solar Mean Anomaly
  const M_rise = (0.9856 * t_rise) - 3.289;
  const M_set = (0.9856 * t_set) - 3.289;

  // 4. Calculate Solar True Longitude
  let L_rise = M_rise + (1.916 * Math.sin(M_rise * Math.PI / 180.0)) + (0.020 * Math.sin(2.0 * M_rise * Math.PI / 180.0)) + 282.634;
  let L_set = M_set + (1.916 * Math.sin(M_set * Math.PI / 180.0)) + (0.020 * Math.sin(2.0 * M_set * Math.PI / 180.0)) + 282.634;
  
  L_rise = (L_rise + 360.0) % 360.0;
  L_set = (L_set + 360.0) % 360.0;

  // 5. Calculate Right Ascension
  let RA_rise = 180.0 / Math.PI * Math.atan(0.91764 * Math.tan(L_rise * Math.PI / 180.0));
  let RA_set = 180.0 / Math.PI * Math.atan(0.91764 * Math.tan(L_set * Math.PI / 180.0));
  
  RA_rise = (RA_rise + 360.0) % 360.0;
  RA_set = (RA_set + 360.0) % 360.0;

  // Adjust quadrants
  const Lquadrant_rise = (Math.floor(L_rise / 90.0)) * 90.0;
  const RAquadrant_rise = (Math.floor(RA_rise / 90.0)) * 90.0;
  RA_rise = RA_rise + (Lquadrant_rise - RAquadrant_rise);
  
  const Lquadrant_set = (Math.floor(L_set / 90.0)) * 90.0;
  const RAquadrant_set = (Math.floor(RA_set / 90.0)) * 90.0;
  RA_set = RA_set + (Lquadrant_set - RAquadrant_set);

  // Convert RA to hours
  RA_rise = RA_rise / 15.0;
  RA_set = RA_set / 15.0;

  // 6. Calculate Solar Declination
  const sinDec_rise = 0.39782 * Math.sin(L_rise * Math.PI / 180.0);
  const cosDec_rise = Math.cos(Math.asin(sinDec_rise));
  
  const sinDec_set = 0.39782 * Math.sin(L_set * Math.PI / 180.0);
  const cosDec_set = Math.cos(Math.asin(sinDec_set));

  // 7. Calculate Solar Hour Angle (zenith = 90.83 degrees for standard sunrise/sunset)
  const zenithRad = 90.83 * Math.PI / 180.0;
  const latRad = latitude * Math.PI / 180.0;
  
  const cosH_rise = (Math.cos(zenithRad) - (sinDec_rise * Math.sin(latRad))) / (cosDec_rise * Math.cos(latRad));
  const cosH_set = (Math.cos(zenithRad) - (sinDec_set * Math.sin(latRad))) / (cosDec_set * Math.cos(latRad));

  if (cosH_rise > 1.0 || cosH_rise < -1.0 || cosH_set > 1.0 || cosH_set < -1.0) {
    return { sunrise: null, sunset: null }; // sun never rises/sets at this day
  }

  // 8. Calculate local mean time of rise/set
  const H_rise = (360.0 - (180.0 / Math.PI * Math.acos(cosH_rise))) / 15.0;
  const T_rise = H_rise + RA_rise - (0.06571 * t_rise) - 6.622;
  const UT_rise = (T_rise - lngHour + 24.0) % 24.0;
  
  const H_set = (180.0 / Math.PI * Math.acos(cosH_set)) / 15.0;
  const T_set = H_set + RA_set - (0.06571 * t_set) - 6.622;
  const UT_set = (T_set - lngHour + 24.0) % 24.0;

  // 9. Convert UT to local timezone (Turkey Time: UTC+3)
  const localOffset = 3.0;
  const local_rise_hours = (UT_rise + localOffset + 24.0) % 24.0;
  const local_set_hours = (UT_set + localOffset + 24.0) % 24.0;

  return {
    sunrise: formatTime(local_rise_hours),
    sunset: formatTime(local_set_hours)
  };
}

function formatTime(hoursDecimal) {
  const hours = Math.floor(hoursDecimal);
  const minutes = Math.round((hoursDecimal - hours) * 60.0) % 60;
  // Handle overflow of minutes to hour
  const finalHours = minutes === 0 && (hoursDecimal - hours) * 60.0 >= 59.5 ? (hours + 1) % 24 : hours;
  return `${String(finalHours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}`;
}

module.exports = {
  getSolarTimes
};
