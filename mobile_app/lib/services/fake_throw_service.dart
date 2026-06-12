import 'dart:math';

import '../models/throw_data.dart';

class FakeThrowService {

  static final Random random = Random();

  static ThrowData generateThrow(int id) {

    final labels = [
      "Backhand",
      "Forehand",
      "Hammer",
      "Scoober",
    ];

    return ThrowData(
      throwId: id,
      label: labels[random.nextInt(labels.length)],
      flightTime: 1.5 + random.nextDouble() * 4.0,
      maxAccel: 2.0 + random.nextDouble() * 8.0,
      maxGyro: 100 + random.nextDouble() * 500,
    );
  }
}