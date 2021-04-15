#!/usr/bin/env python

"""
    Script to combine several event definition files
"""

import argparse
import json

def main():
    """ main method """
    # Parse command line arguments
    parser = argparse.ArgumentParser(description="Combine event definition files.")
    parser.add_argument("files",
                        default=[],
                        metavar="EVENT.JSON",
                        nargs='+',
                        help="one or more event definition files")

    parser.add_argument('-o', '--output',
                        type=str,
                        action='store',
                        help='Output file',
                        required=True)

    args = parser.parse_args()
    input_files = args.files
    output_file = args.output

    events = {}
    for input_file in input_files:
        with open(input_file, 'r') as json_file:
            new_events = json.load(json_file)
            assert "version" in new_events
            assert new_events["version"] == 1

            if not events:
                events = new_events
                continue

            # merge new_events into events
            for component in new_events["components"]:
                comp_id = component["component_id"]
                matching_comp = [c for c in events["components"] if comp_id == c["component_id"]]
                if matching_comp:
                    matching_comp = matching_comp[0]

                    # same component already exists: try to merge

                    assert matching_comp["namespace"] == component["namespace"], \
                        "Namespaces with equal component ID's must match"

                    # enums
                    for enum in component.get("enums", []):
                        if not "enums" in matching_comp:
                            matching_comp["enums"] = []
                        assert not any(enum["name"] == e["name"] \
                            for e in matching_comp["enums"]), \
                            "enum collision: {:}".format(enum["name"])
                        matching_comp["enums"].append(enum)

                    # event groups
                    for group in component.get("event_groups", []):
                        group_name = group["name"]
                        if "event_groups" in matching_comp:
                            matching_group = [g for g in matching_comp["event_groups"]
                                              if group_name == g["name"]]
                        else:
                            matching_group = None

                        if matching_group:
                            matching_group = matching_group[0]
                            # events
                            for event in group["events"]:
                                assert not any(event["sub_id"] == e["sub_id"] \
                                    for e in matching_group["events"]), \
                                    "ID collision: {:}".format(event["sub_id"])
                                assert not any(event["name"] == e["name"] \
                                    for e in matching_group["events"]), \
                                    "event name collision: {:}".format(event["name"])
                                matching_group["events"].append(event)
                        else:
                            matching_comp["event_groups"].append(group)

                    if "supported_protocols" in component:
                        matching_comp["supported_protocols"] = \
                            list(set(matching_comp.get("supported_protocols", []) +
                            component["supported_protocols"]))
                else:
                    events["components"].append(component)

    with open(output_file, 'w') as f:
        f.write(json.dumps(events, indent=2))

if __name__ == "__main__":
    main()

