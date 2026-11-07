"""TASK-1625: the agent instruction files must not drift apart.

CLAUDE.md (Claude Code), AGENTS.md (Codex) and .github/copilot-instructions.md (GitHub
Copilot) carry the block between the AGENT-CORE markers word for word. Before TASK-1625
they contradicted each other: AGENTS.md made the backlog MCP server the primary interface,
which CLAUDE.md forbids, and the Copilot file still described ESP8266 firmware with
ArduinoJson, which ADR-146 forbids on this line.

Mirrors tests/test_sat_ground_truth_block.py and tests/test_adr_governance.py.
Run: python tests/test_agent_core_block.py
"""
import os
import re
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILES = ("CLAUDE.md", "AGENTS.md", os.path.join(".github", "copilot-instructions.md"))
START = "<!-- AGENT-CORE START"
END = "<!-- AGENT-CORE END -->"


def read(name):
    with open(os.path.join(REPO, name), encoding="utf-8") as f:
        return f.read().replace("\r\n", "\n")


def block(name):
    text = read(name)
    i, j = text.find(START), text.find(END)
    if i < 0 or j < 0:
        return None
    return text[i:j + len(END)]


class AgentCoreBlock(unittest.TestCase):
    def test_every_file_carries_the_block(self):
        for name in FILES:
            self.assertIsNotNone(block(name), name + " has no AGENT-CORE block")

    def test_blocks_are_identical(self):
        claude = block("CLAUDE.md")
        for name in FILES[1:]:
            self.assertEqual(block(name), claude, "the AGENT-CORE block differs between CLAUDE.md and " + name)

    def test_block_states_the_shared_rules(self):
        b = block("CLAUDE.md") or ""
        for rule in ("CLAUDE.md wins", "`dev` is the 2.x line: ESP32-S3 only", "`backlog` CLI",
                     "Never use the backlog MCP server", "No ArduinoJson (ADR-146)", "No HTTPS or WSS",
                     "addCommandToQueue()", "bin/bump-prerelease.sh", "needs-maintainer-ok",
                     "docs/sat/sergeant-d-corrections.md", "at most 2 API calls"):
            self.assertIn(rule, b, "rule missing from the AGENT-CORE block: " + rule)

    def test_agents_md_does_not_prefer_the_backlog_mcp_server(self):
        text = read("AGENTS.md")
        for phrase in ("MCP is the preferred interface", "Use Backlog MCP tools as the primary interface",
                       "Backlog MCP tools first"):
            self.assertNotIn(phrase, text, "AGENTS.md still prefers the backlog MCP server: " + phrase)

    def test_copilot_instructions_describe_the_2x_line(self):
        text = read(os.path.join(".github", "copilot-instructions.md"))
        self.assertNotIn("This is the ESP8266 firmware", text)
        self.assertIsNone(re.search(r"^\s*- ArduinoJson\b", text, re.M), "ArduinoJson listed as a library")
        self.assertNotIn("ESP8266 Arduino Core 2.7.4", text)

    def test_claude_md_points_at_existing_discord_guide(self):
        # CLAUDE.md sends readers to the Discord MCP guide; it has to exist.
        self.assertIn("docs/guides/discord-mcp-server.md", read("CLAUDE.md"))
        self.assertTrue(os.path.exists(os.path.join(REPO, "docs", "guides", "discord-mcp-server.md")),
                        "docs/guides/discord-mcp-server.md is missing")


if __name__ == "__main__":
    unittest.main(verbosity=2)
