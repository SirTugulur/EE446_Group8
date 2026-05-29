class ThrowData {

  final int throwId;
  String label;

  final double flightTime;
  final double maxAccel;
  final double maxGyro;

  bool wobble;

  ThrowData({
    required this.throwId,
    required this.label,
    required this.flightTime,
    required this.maxAccel,
    required this.maxGyro,
    this.wobble = false,
  });
}