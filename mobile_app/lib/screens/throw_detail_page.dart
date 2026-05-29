import 'package:flutter/material.dart';

import '../models/throw_data.dart';

class ThrowDetailPage extends StatelessWidget {
  final ThrowData throwData;

  const ThrowDetailPage({
    super.key,
    required this.throwData,
  });

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text(throwData.label)),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Text(
            "Throw #${throwData.throwId}",
            style: Theme.of(context).textTheme.headlineSmall,
          ),
          const SizedBox(height: 12),
          _MetricRow(
            label: "Flight Time",
            value: "${throwData.flightTime.toStringAsFixed(2)} s",
          ),
          _MetricRow(
            label: "Max Accel",
            value: "${throwData.maxAccel.toStringAsFixed(2)} g",
          ),
          _MetricRow(
            label: "Max Gyro",
            value: throwData.maxGyro.toStringAsFixed(0),
          ),
          _MetricRow(
            label: "Wobbly",
            value: throwData.wobble ? "Yes" : "No",
          ),
          _MetricRow(
            label: "Samples",
            value: throwData.samples.length.toString(),
          ),
          const SizedBox(height: 20),
          Text(
            "Flight Data",
            style: Theme.of(context).textTheme.titleLarge,
          ),
          const SizedBox(height: 8),
          SizedBox(
            height: 220,
            child: DecoratedBox(
              decoration: BoxDecoration(
                border: Border.all(color: Theme.of(context).dividerColor),
                borderRadius: BorderRadius.circular(8),
              ),
              child: Center(
                child: Text(
                  throwData.samples.isEmpty
                      ? "No sample data available"
                      : "Graph-ready sample data captured",
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _MetricRow extends StatelessWidget {
  final String label;
  final String value;

  const _MetricRow({
    required this.label,
    required this.value,
  });

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label),
          Text(
            value,
            style: const TextStyle(fontWeight: FontWeight.bold),
          ),
        ],
      ),
    );
  }
}
