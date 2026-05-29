class ThrowSample {
  final int sampleIndex;
  final int throwId;
  final String label;
  final int timeMs;
  final double ax, ay, az;
  final double gx, gy, gz;
  final double mx, my, mz;
  final double accelMag;
  final double gyroMag;

  ThrowSample({
    required this.sampleIndex,
    required this.throwId,
    required this.label,
    required this.timeMs,
    required this.ax,
    required this.ay,
    required this.az,
    required this.gx,
    required this.gy,
    required this.gz,
    required this.mx,
    required this.my,
    required this.mz,
    required this.accelMag,
    required this.gyroMag,
  });
}

class ThrowData {
  final int throwId;
  String label;

  final double flightTime;
  final double maxAccel;
  final double maxGyro;
  final List<ThrowSample> samples;

  bool wobble;

  ThrowData({
    required this.throwId,
    required this.label,
    required this.flightTime,
    required this.maxAccel,
    required this.maxGyro,
    this.samples = const [],
    this.wobble = false,
  });
}
