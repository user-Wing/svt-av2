"""Generate the exact integer Q12 cost lookup used by rate estimation."""
import argparse
from pathlib import Path


def generate(dest):
    costs = [0]  # Probability zero is rejected before the lookup.
    for probability in range(1, 32769):
        exponent = probability.bit_length() - 1
        normalized = probability << (31 - exponent)
        fraction = 0
        for _ in range(12):
            normalized = (normalized * normalized) >> 31
            fraction <<= 1
            if normalized >= 1 << 32:
                normalized >>= 1
                fraction |= 1
        costs.append((15 - exponent) * 4096 - fraction)
    assert max(costs) == 61440 and costs[-1] == 0
    text = '/* Exact Q12 costs for p15=0..32768; index zero is unused. */\n'
    text += 'static const uint16_t k_probability_cost_q12[32769] = {\n'
    text += ''.join('    ' + ','.join(map(str, costs[i:i + 16])) + ',\n'
                    for i in range(0, len(costs), 16))
    text += '};\n'
    dest.write_text(text, encoding='utf-8', newline='\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--dest', type=Path, required=True)
    generate(parser.parse_args().dest)
