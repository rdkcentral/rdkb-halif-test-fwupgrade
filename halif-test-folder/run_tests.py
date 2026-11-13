"""Project entry point for the reusable controller runner."""
from framework.configuration import create_settings
from framework.app_runner import main

if __name__ == "__main__":
    raise SystemExit(main(settings=create_settings()))

