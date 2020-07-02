#!/usr/bin/env python3

import argparse
import random


def gererate_benchmark_output(num_operations, range_max, max_num_equal):
    randon_nums = dict()

    for i in range(num_operations):
        irand = random.randrange(1, range_max, 1)
        value = randon_nums.get(irand, None)
        if value is None:
            randon_nums[irand] = 1
            print("+{}".format(irand))
        elif value == max_num_equal:
            randon_nums.pop(irand)
            print("-{}".format(irand))
        else:
            randon_nums[irand] = value + 1
            print("?{}".format(irand))


def setup_arg_parser():
    """
    Set up argument parser and default values
    :return: parsed argument list
    """
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--num-operation',
        type=int,
        nargs='?',
        default=20000000,
        help='number of hash table operation which will be generated.'
    )
    parser.add_argument(
        '--key-max',
        type=int,
        nargs='?',
        default=250000,
        help='generated keys are in the range of 1..key-max'
    )
    parser.add_argument(
        '--max-lookups-per-key',
        type=int,
        nargs='?',
        default=30,
        help='after this number of look ups for a specific key is reached, this key will be removed.'
    )

    return parser.parse_args()


def main():
    args = setup_arg_parser()
    gererate_benchmark_output(args.num_operation, args.key_max, args.max_lookups_per_key)


if __name__ == "__main__":
    main()
