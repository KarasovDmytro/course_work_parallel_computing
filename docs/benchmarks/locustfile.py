"""Locust load for E2: every simulated client sends POST /api/search again and again.

The search mode comes from the SEARCH_MODE environment variable (keyword or expanded).
Clients do not wait between requests (closed loop): the number of clients is the number of
requests in flight, and throughput shows how many of them the server finishes per second.

In expanded mode a response without expanded_terms (LLM queue timeout or Ollama error) is
counted as a failure with the message "no expanded terms", so the fallback share is visible
in the Locust statistics. Real errors keep their HTTP status.
"""

import os
import random

from locust import HttpUser, constant, task

MODE = os.environ.get("SEARCH_MODE", "keyword")

QUERIES = [
    "whales hunting", "storage battery", "running horses", "sailing ships", "ancient kingdoms",
    "beautiful flowers", "bloody battles", "loving mothers", "old castle ghost", "steam engine",
    "gold mine", "winter snow", "king and queen", "french revolution", "electric light",
    "pirate treasure", "railway station", "desert island", "village church", "doctor medicine",
]


class SearchUser(HttpUser):
    wait_time = constant(0)

    @task
    def search(self):
        body = {"query": random.choice(QUERIES), "mode": MODE, "limit": 10}
        with self.client.post("/api/search", json=body, name=f"search {MODE}", catch_response=True) as response:
            if response.status_code != 200:
                response.failure(f"HTTP {response.status_code}")
            elif MODE == "expanded" and not response.json().get("expanded_terms"):
                response.failure("no expanded terms")
            else:
                response.success()
