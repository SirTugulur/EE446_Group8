import 'package:flutter/material.dart';

import '../models/throw_data.dart';

class StatsPage extends StatelessWidget {
  final List<ThrowData> savedThrows;
  final List<String> throwTypes;
  final bool showAppBar;

  const StatsPage({
    super.key,
    required this.savedThrows,
    required this.throwTypes,
    this.showAppBar = true,
  });

  @override
  Widget build(BuildContext context) {
    final observedTypes = savedThrows.map((throwData) => throwData.label);
    final typeLabels = {...throwTypes, ...observedTypes}.toList()..sort();
    final stats = typeLabels.map((type) {
      final throws = savedThrows.where((throwData) => throwData.label == type);
      return _ThrowTypeStats(type, throws.toList());
    }).toList();

    // Filter out throw types that don't have any data yet
    final populatedStats = stats.where((stat) => stat.count > 0).toList();

    // Best throw = Highest average max gyro
    final best = populatedStats.isEmpty
        ? null
        : populatedStats.reduce((a, b) => a.averageMaxGyro >= b.averageMaxGyro ? a : b);
        
    // Worst throw = Lowest average max gyro
    final worst = populatedStats.isEmpty
        ? null
        : populatedStats.reduce((a, b) => a.averageMaxGyro <= b.averageMaxGyro ? a : b);

    final body = ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Row(
            children: [
              Expanded(
                child: _StatSummaryCard(
                  title: "Best Throw",
                  value: best == null
                      ? "Not enough data"
                      : "${best.type} (${best.averageMaxGyro.toStringAsFixed(0)} gyro)",
                  icon: Icons.trending_up,
                  color: Colors.green,
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: _StatSummaryCard(
                  title: "Worst Throw",
                  value: worst == null
                      ? "Not enough data"
                      : "${worst.type} (${worst.averageMaxGyro.toStringAsFixed(0)} gyro)",
                  icon: Icons.trending_down,
                  color: Colors.orange,
                ),
              ),
            ],
          ),
          
          const SizedBox(height: 20),
          Text(
            "Throw Types",
            style: Theme.of(context).textTheme.titleLarge,
          ),
          const SizedBox(height: 8),
          for (final stat in stats) _ThrowTypeStatsTile(stat: stat),
        ],
    );

    if (!showAppBar) {
      return body;
    }

    return Scaffold(
      appBar: AppBar(title: const Text("Throw Classification")),
      body: body,
    );
  }

  String _percent(double value) {
    return "${(value * 100).toStringAsFixed(0)}%";
  }
}

class _ThrowTypeStats {
  final String type;
  final List<ThrowData> throws;

  _ThrowTypeStats(this.type, this.throws);

  int get count => throws.length;

  double get averageFlightTime => _average((throwData) => throwData.flightTime);

  double get averageMaxAccel => _average((throwData) => throwData.maxAccel);

  double get averageMaxGyro => _average((throwData) => throwData.maxGyro);

  double? get completionRate {
    final knownThrows = throws
        .where((throwData) => throwData.completed != null)
        .toList(growable: false);

    if (knownThrows.isEmpty) {
      return null;
    }

    return knownThrows.where((throwData) => throwData.completed!).length /
        knownThrows.length;
  }

  double _average(double Function(ThrowData throwData) valueForThrow) {
    if (throws.isEmpty) {
      return 0;
    }

    final total = throws.fold<double>(
      0,
      (sum, throwData) => sum + valueForThrow(throwData),
    );

    return total / throws.length;
  }
}

class _ThrowTypeStatsTile extends StatelessWidget {
  final _ThrowTypeStats stat;

  const _ThrowTypeStatsTile({required this.stat});

  @override
  Widget build(BuildContext context) {
    return Card(
      child: ListTile(
        leading: const Icon(Icons.sports),
        title: Text(stat.type),
        subtitle: Text(
          "Throws: ${stat.count}\n"
          "Complete: ${_completionText(stat.completionRate)}\n"
          "Avg flight: ${stat.averageFlightTime.toStringAsFixed(2)} s | "
          "Avg max gyro: ${stat.averageMaxGyro.toStringAsFixed(0)}",
        ),
        isThreeLine: true,
      ),
    );
  }

  String _completionText(double? rate) {
    if (rate == null) {
      return "Unknown";
    }

    return "${(rate * 100).toStringAsFixed(0)}%";
  }
}

class _StatSummaryCard extends StatelessWidget {
  final String title;
  final String value;
  final IconData icon;
  final Color? color;

  const _StatSummaryCard({
    required this.title,
    required this.value,
    required this.icon,
    this.color,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Icon(icon, color: color),
            const SizedBox(height: 10),
            Text(title, style: Theme.of(context).textTheme.labelLarge),
            const SizedBox(height: 4),
            Text(
              value,
              style: Theme.of(context).textTheme.titleMedium,
            ),
          ],
        ),
      ),
    );
  }
}
