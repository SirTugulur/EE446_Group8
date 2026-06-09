import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../models/throw_data.dart';

enum ThrowGraphMetric {
  accel,
  gyro,
}

class ThrowGraphCard extends StatefulWidget {
  final ThrowData throwData;

  const ThrowGraphCard({
    super.key,
    required this.throwData,
  });

  @override
  State<ThrowGraphCard> createState() => _ThrowGraphCardState();
}

class _ThrowGraphCardState extends State<ThrowGraphCard> {
  ThrowGraphMetric metric = ThrowGraphMetric.accel;

  @override
  Widget build(BuildContext context) {
    final samples = widget.throwData.samples;

    return Card(
      margin: EdgeInsets.zero,
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Expanded(
                  child: Text(
                    "Flight Data",
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                ),
                SegmentedButton<ThrowGraphMetric>(
                  segments: const [
                    ButtonSegment(
                      value: ThrowGraphMetric.accel,
                      label: Text("Accel"),
                      icon: Icon(Icons.speed),
                    ),
                    ButtonSegment(
                      value: ThrowGraphMetric.gyro,
                      label: Text("Gyro"),
                      icon: Icon(Icons.rotate_right),
                    ),
                  ],
                  selected: {metric},
                  onSelectionChanged: (selection) {
                    setState(() {
                      metric = selection.first;
                    });
                  },
                ),
              ],
            ),
            const SizedBox(height: 12),
            SizedBox(
              height: 240,
              width: double.infinity,
              child: samples.isEmpty
                  ? const Center(child: Text("No sample data available"))
                  : CustomPaint(
                      painter: _ThrowGraphPainter(
                        samples: samples,
                        metric: metric,
                        color: Theme.of(context).colorScheme.primary,
                        gridColor: Theme.of(context).dividerColor,
                        labelColor: Theme.of(context).colorScheme.onSurface,
                      ),
                    ),
            ),
          ],
        ),
      ),
    );
  }
}

class _ThrowGraphPainter extends CustomPainter {
  final List<ThrowSample> samples;
  final ThrowGraphMetric metric;
  final Color color;
  final Color gridColor;
  final Color labelColor;

  _ThrowGraphPainter({
    required this.samples,
    required this.metric,
    required this.color,
    required this.gridColor,
    required this.labelColor,
  });

  @override
  void paint(Canvas canvas, Size size) {
    const left = 42.0;
    const right = 12.0;
    const top = 12.0;
    const bottom = 32.0;
    final chart = Rect.fromLTWH(
      left,
      top,
      math.max(0, size.width - left - right),
      math.max(0, size.height - top - bottom),
    );

    final values = samples.map(_valueForSample).toList(growable: false);
    final minTime = samples.first.timeMs.toDouble();
    final maxTime = samples.last.timeMs.toDouble();
    final maxValue = math.max(1.0, values.reduce(math.max));
    final minValue = math.min(0.0, values.reduce(math.min));
    final valueRange = math.max(0.001, maxValue - minValue);
    final timeRange = math.max(1.0, maxTime - minTime);

    final gridPaint = Paint()
      ..color = gridColor
      ..strokeWidth = 1;
    final axisPaint = Paint()
      ..color = labelColor.withValues(alpha: 0.55)
      ..strokeWidth = 1.2;

    for (var i = 0; i <= 4; i++) {
      final y = chart.top + chart.height * i / 4;
      canvas.drawLine(Offset(chart.left, y), Offset(chart.right, y), gridPaint);
    }

    for (var i = 0; i <= 4; i++) {
      final x = chart.left + chart.width * i / 4;
      canvas.drawLine(Offset(x, chart.top), Offset(x, chart.bottom), gridPaint);
    }

    canvas.drawLine(
      Offset(chart.left, chart.bottom),
      Offset(chart.right, chart.bottom),
      axisPaint,
    );
    canvas.drawLine(
      Offset(chart.left, chart.top),
      Offset(chart.left, chart.bottom),
      axisPaint,
    );

    final path = Path();
    for (var i = 0; i < samples.length; i++) {
      final sample = samples[i];
      final x = chart.left + ((sample.timeMs - minTime) / timeRange) * chart.width;
      final y =
          chart.bottom - ((values[i] - minValue) / valueRange) * chart.height;

      if (i == 0) {
        path.moveTo(x, y);
      } else {
        path.lineTo(x, y);
      }
    }

    final linePaint = Paint()
      ..color = color
      ..strokeWidth = 2.4
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round;
    canvas.drawPath(path, linePaint);

    _drawLabel(canvas, maxValue.toStringAsFixed(1), Offset(0, chart.top));
    _drawLabel(
      canvas,
      minValue.toStringAsFixed(1),
      Offset(0, chart.bottom - 14),
    );
    _drawLabel(
      canvas,
      "${((maxTime - minTime) / 1000).toStringAsFixed(2)}s",
      Offset(chart.right - 34, chart.bottom + 8),
    );
    _drawLabel(canvas, "0s", Offset(chart.left, chart.bottom + 8));
  }

  double _valueForSample(ThrowSample sample) {
    switch (metric) {
      case ThrowGraphMetric.accel:
        return sample.accelMag;
      case ThrowGraphMetric.gyro:
        return sample.gyroMag;
    }
  }

  void _drawLabel(Canvas canvas, String text, Offset offset) {
    final painter = TextPainter(
      text: TextSpan(
        text: text,
        style: TextStyle(
          color: labelColor.withValues(alpha: 0.72),
          fontSize: 11,
        ),
      ),
      textDirection: TextDirection.ltr,
    )..layout(maxWidth: 42);

    painter.paint(canvas, offset);
  }

  @override
  bool shouldRepaint(covariant _ThrowGraphPainter oldDelegate) {
    return oldDelegate.samples != samples ||
        oldDelegate.metric != metric ||
        oldDelegate.color != color ||
        oldDelegate.gridColor != gridColor ||
        oldDelegate.labelColor != labelColor;
  }
}
