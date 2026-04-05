#!/usr/bin/env python3
"""
OpenTherm.eu public crawler

Doel:
- Crawl alleen publiek toegankelijke pagina's van https://www.opentherm.eu/
- Verzamel vendor/member-informatie, documentlinks, ID-archieven en product-/detailpagina's
- Bouw een JSON-bestand dat bruikbaar is als input voor firmware

Belangrijke uitgangspunten:
- Geen login, geen members area, geen private content
- Polite crawling met delay
- Beperkt tot hetzelfde domein
- Heuristische merkextractie is gemarkeerd als zodanig
- Officiële fabrikantmapping blijft primair gebaseerd op de publieke "Members ID Codes"-bron

Gebruik:
    python opentherm_public_crawler.py --output opentherm_public.json
    python opentherm_public_crawler.py --max-pages 400 --delay 0.75 --verbose

Optionele packages:
    pip install requests beautifulsoup4

Opmerking:
- Dit script probeert niet om PDF-inhoud te OCR-en of zwaar te reverse engineeren.
- Het JSON-model is bewust expliciet zodat firmware-build tooling er later eenvoudig uit kan lezen.
"""

from __future__ import annotations

import argparse
import json
import logging
import re
import sys
import time
from collections import deque
from dataclasses import dataclass, asdict, field
from typing import Dict, List, Optional, Set, Tuple
from urllib.parse import urljoin, urlparse, urldefrag
from urllib.robotparser import RobotFileParser

import requests
from bs4 import BeautifulSoup

BASE_URL = "https://www.opentherm.eu/"
ALLOWED_HOSTS = {"www.opentherm.eu", "opentherm.eu"}

DEFAULT_HEADERS = {
    "User-Agent": "OpenThermPublicCrawler/1.0 (+public data collection for firmware mapping)",
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
    "Accept-Language": "en-US,en;q=0.8,nl;q=0.7",
}


@dataclass
class PageRecord:
    url: str
    title: Optional[str] = None
    category: Optional[str] = None
    discovered_from: Optional[str] = None


@dataclass
class MemberEntry:
    company_name: str
    url: Optional[str] = None
    country: Optional[str] = None
    confidence: str = "public-site-direct"
    source_url: Optional[str] = None


@dataclass
class DocumentEntry:
    title: str
    url: str
    pdf_url: Optional[str] = None
    source_url: Optional[str] = None
    tags: List[str] = field(default_factory=list)


@dataclass
class DataIdHint:
    data_id: Optional[int]
    data_id_label: Optional[str]
    brand: Optional[str]
    product: Optional[str]
    product_url: Optional[str]
    confidence: str
    source_url: Optional[str]


def normalize_url(url: str, base: Optional[str] = None) -> Optional[str]:
    if not url:
        return None
    abs_url = urljoin(base or BASE_URL, url)
    abs_url, _frag = urldefrag(abs_url)
    parsed = urlparse(abs_url)
    if parsed.scheme not in {"http", "https"}:
        return None
    if parsed.netloc not in ALLOWED_HOSTS:
        return None
    cleaned = parsed._replace(query="").geturl()
    return cleaned


def is_html_like(content_type: str, url: str) -> bool:
    content_type = (content_type or "").lower()
    return "text/html" in content_type or url.endswith("/")


def classify_url(url: str) -> str:
    parsed = urlparse(url)
    path = parsed.path.rstrip("/") or "/"

    if path == "/members":
        return "members_index"
    if path.startswith("/ids/"):
        return "data_id_archive"
    if path.startswith("/document/"):
        return "document"
    if path.startswith("/product/") or path.startswith("/products/"):
        return "product"
    if path.startswith("/member/"):
        return "member_detail"
    return "page"


def extract_title(soup: BeautifulSoup) -> Optional[str]:
    if soup.title and soup.title.string:
        return soup.title.string.strip()
    h1 = soup.find(["h1", "h2"])
    if h1:
        return h1.get_text(" ", strip=True)
    return None


def text_of(el) -> str:
    return el.get_text(" ", strip=True) if el else ""


def guess_brand_from_text(text: str, known_brands: Set[str]) -> Optional[str]:
    lower = text.lower()
    for brand in sorted(known_brands, key=len, reverse=True):
        if brand and brand.lower() in lower:
            return brand
    return None


def extract_possible_country(text: str) -> Optional[str]:
    # Lichte heuristiek: sommige memberlijsten tonen landnamen expliciet.
    # Houd het conservatief.
    common = [
        "Netherlands", "Germany", "Italy", "Belgium", "France", "United Kingdom",
        "Sweden", "Austria", "Turkey", "China", "South Korea", "Czech Republic",
        "Poland", "Spain", "Switzerland", "Ireland", "Denmark", "Portugal",
    ]
    for country in common:
        if country.lower() in text.lower():
            return country
    return None


class OpenThermCrawler:
    def __init__(
        self,
        base_url: str,
        delay: float = 0.75,
        timeout: float = 20.0,
        max_pages: int = 400,
        verbose: bool = False,
    ) -> None:
        self.base_url = base_url
        self.delay = delay
        self.timeout = timeout
        self.max_pages = max_pages

        self.session = requests.Session()
        self.session.headers.update(DEFAULT_HEADERS)

        self.seen: Set[str] = set()
        self.pages: List[PageRecord] = []
        self.members: Dict[str, MemberEntry] = {}
        self.documents: Dict[str, DocumentEntry] = {}
        self.data_id_hints: List[DataIdHint] = []
        self.product_pages: Dict[str, Dict] = {}
        self.errors: List[Dict[str, str]] = []

        self.robot_parser = RobotFileParser()
        self.robot_parser.set_url(urljoin(self.base_url, "/robots.txt"))
        try:
            self.robot_parser.read()
        except Exception:
            # Niet fataal; we blijven conservatief.
            pass

        logging.basicConfig(
            level=logging.DEBUG if verbose else logging.INFO,
            format="%(levelname)s: %(message)s",
        )

    def allowed_by_robots(self, url: str) -> bool:
        try:
            return self.robot_parser.can_fetch(DEFAULT_HEADERS["User-Agent"], url)
        except Exception:
            return True

    def fetch(self, url: str) -> Optional[requests.Response]:
        if not self.allowed_by_robots(url):
            logging.info("Overgeslagen door robots.txt: %s", url)
            return None
        try:
            time.sleep(self.delay)
            resp = self.session.get(url, timeout=self.timeout)
            if resp.status_code >= 400:
                self.errors.append({"url": url, "error": f"HTTP {resp.status_code}"})
                logging.warning("HTTP %s voor %s", resp.status_code, url)
                return None
            return resp
        except Exception as exc:
            self.errors.append({"url": url, "error": str(exc)})
            logging.warning("Fout bij ophalen %s: %s", url, exc)
            return None

    def crawl(self) -> None:
        queue: deque[Tuple[str, Optional[str]]] = deque()
        queue.append((self.base_url, None))

        while queue and len(self.seen) < self.max_pages:
            url, discovered_from = queue.popleft()
            if url in self.seen:
                continue
            self.seen.add(url)

            resp = self.fetch(url)
            if not resp:
                continue

            content_type = resp.headers.get("Content-Type", "")
            if not is_html_like(content_type, url):
                continue

            soup = BeautifulSoup(resp.text, "html.parser")
            category = classify_url(url)
            title = extract_title(soup)
            self.pages.append(PageRecord(url=url, title=title, category=category, discovered_from=discovered_from))
            logging.info("Verwerkt [%s]: %s", category, url)

            # Gespecialiseerde parsers
            if category == "members_index":
                self.parse_members_page(url, soup)
            elif category == "document":
                self.parse_document_page(url, soup)
            elif category == "data_id_archive":
                self.parse_data_id_page(url, soup)
            elif category == "product":
                self.parse_product_page(url, soup)

            # Link discovery
            for a in soup.find_all("a", href=True):
                href = normalize_url(a["href"], base=url)
                if not href:
                    continue

                # Sla private areas expliciet over
                if "/faq/" in href or "/members-area/" in href:
                    continue

                # Neem relevante publieke secties zeker mee
                parsed = urlparse(href)
                path = parsed.path.lower()
                if (
                    href.startswith(self.base_url)
                    and (
                        path == "/"
                        or path.startswith("/members")
                        or path.startswith("/document/")
                        or path.startswith("/ids/")
                        or path.startswith("/product/")
                        or path.startswith("/products/")
                        or path.startswith("/member/")
                        or path.startswith("/opentherm-protocol/")
                    )
                ):
                    if href not in self.seen:
                        queue.append((href, url))

    def parse_members_page(self, source_url: str, soup: BeautifulSoup) -> None:
        # Generieke aanpak: pak links binnen content, filter op vermoedelijke member entries.
        content = soup.find("main") or soup.find("article") or soup.body
        if not content:
            return

        for a in content.find_all("a", href=True):
            name = text_of(a)
            href = normalize_url(a["href"], base=source_url)
            if not name or not href:
                continue
            if len(name) < 2:
                continue

            # Veel WordPress-membersites linken naar detailpagina's of anchors.
            around = a.parent.get_text(" ", strip=True) if a.parent else name
            country = extract_possible_country(around)

            entry = self.members.get(name.lower())
            if not entry:
                self.members[name.lower()] = MemberEntry(
                    company_name=name,
                    url=href,
                    country=country,
                    confidence="public-site-direct",
                    source_url=source_url,
                )

    def parse_document_page(self, source_url: str, soup: BeautifulSoup) -> None:
        title = extract_title(soup) or source_url.rstrip("/").split("/")[-1]
        pdf_url = None
        tags: List[str] = []

        for a in soup.find_all("a", href=True):
            href = normalize_url(a["href"], base=source_url)
            if not href:
                continue
            if href.lower().endswith(".pdf"):
                pdf_url = href
                break

        # Probeer simpele tag-hints uit titel/tekst
        page_text = soup.get_text(" ", strip=True).lower()
        for tag in ["member id", "id code", "protocol", "document", "members"]:
            if tag in page_text or tag in title.lower():
                tags.append(tag)

        self.documents[source_url] = DocumentEntry(
            title=title,
            url=source_url,
            pdf_url=pdf_url,
            source_url=source_url,
            tags=sorted(set(tags)),
        )

    def parse_data_id_page(self, source_url: str, soup: BeautifulSoup) -> None:
        title = extract_title(soup) or ""
        text = soup.get_text(" ", strip=True)

        data_id = None
        data_id_label = None

        # Voorbeelden: "ID 115: OEM-specific diagnostic/service code"
        m = re.search(r"\bID\s*0*([0-9]{1,3})\b\s*[:\-]?\s*(.*)", title, re.IGNORECASE)
        if m:
            data_id = int(m.group(1))
            maybe_label = m.group(2).strip()
            data_id_label = maybe_label if maybe_label else None
        else:
            m2 = re.search(r"\bID\s*0*([0-9]{1,3})\b", text, re.IGNORECASE)
            if m2:
                data_id = int(m2.group(1))

        known_brands = {entry.company_name for entry in self.members.values()}

        # Zoek product-/detaillinks op deze archiefpagina
        content = soup.find("main") or soup.find("article") or soup.body
        if not content:
            return

        found_any = False
        for a in content.find_all("a", href=True):
            href = normalize_url(a["href"], base=source_url)
            if not href:
                continue
            label = text_of(a)
            if not label:
                continue

            path = urlparse(href).path.lower()
            if path.startswith("/product/") or path.startswith("/products/") or path.startswith("/member/"):
                brand = guess_brand_from_text(label, known_brands)
                confidence = "public-site-direct" if brand else "heuristic"
                self.data_id_hints.append(
                    DataIdHint(
                        data_id=data_id,
                        data_id_label=data_id_label,
                        brand=brand,
                        product=label,
                        product_url=href,
                        confidence=confidence,
                        source_url=source_url,
                    )
                )
                found_any = True

        if not found_any:
            # Fallback: sla minimaal de page-level hint op.
            brand = guess_brand_from_text(text, known_brands)
            self.data_id_hints.append(
                DataIdHint(
                    data_id=data_id,
                    data_id_label=data_id_label,
                    brand=brand,
                    product=None,
                    product_url=None,
                    confidence="heuristic",
                    source_url=source_url,
                )
            )

    def parse_product_page(self, source_url: str, soup: BeautifulSoup) -> None:
        title = extract_title(soup)
        text = soup.get_text(" ", strip=True)
        known_brands = {entry.company_name for entry in self.members.values()}
        brand = guess_brand_from_text(text or (title or ""), known_brands)

        # Productpagina's kunnen extra data-id-links bevatten.
        data_ids: List[int] = []
        for a in soup.find_all("a", href=True):
            href = normalize_url(a["href"], base=source_url)
            if not href:
                continue
            path = urlparse(href).path.lower()
            if path.startswith("/ids/"):
                label = text_of(a)
                m = re.search(r"\bID\s*0*([0-9]{1,3})\b", label, re.IGNORECASE)
                if m:
                    data_ids.append(int(m.group(1)))

        self.product_pages[source_url] = {
            "title": title,
            "brand_guess": brand,
            "linked_data_ids": sorted(set(data_ids)),
            "source_url": source_url,
        }

    def build_json(self) -> Dict:
        members_sorted = sorted(self.members.values(), key=lambda x: x.company_name.lower())
        documents_sorted = sorted(self.documents.values(), key=lambda x: (x.title or "").lower())
        data_id_hints_sorted = sorted(
            self.data_id_hints,
            key=lambda x: (
                9999 if x.data_id is None else x.data_id,
                (x.brand or "").lower(),
                (x.product or "").lower(),
            ),
        )

        # Bouw ook een firmware-vriendelijke index per data_id
        hints_by_data_id: Dict[str, List[Dict]] = {}
        for hint in data_id_hints_sorted:
            key = "unknown" if hint.data_id is None else str(hint.data_id)
            hints_by_data_id.setdefault(key, []).append(
                {
                    "brand": hint.brand,
                    "product": hint.product,
                    "product_url": hint.product_url,
                    "confidence": hint.confidence,
                    "source_url": hint.source_url,
                    "data_id_label": hint.data_id_label,
                }
            )

        # Detecteer kandidaten voor officiële member-id documenten
        official_docs = []
        for doc in documents_sorted:
            title_l = (doc.title or "").lower()
            tags_l = " ".join(doc.tags).lower()
            if "member" in title_l and "id" in title_l or ("member id" in tags_l or "id code" in tags_l):
                official_docs.append(asdict(doc))

        return {
            "meta": {
                "source": self.base_url,
                "crawler": "OpenThermPublicCrawler/1.0",
                "public_only": True,
                "notes": [
                    "Gebruik member/manufacturer ID als primaire merkidentificatie.",
                    "Data-ID/MsgID correlaties uit publieke pagina's zijn heuristisch, tenzij expliciet anders gedocumenteerd.",
                    "Private/members-only content is niet benaderd.",
                ],
                "stats": {
                    "pages_seen": len(self.pages),
                    "members_found": len(members_sorted),
                    "documents_found": len(documents_sorted),
                    "data_id_hints_found": len(data_id_hints_sorted),
                    "product_pages_found": len(self.product_pages),
                    "errors": len(self.errors),
                },
            },
            "official_member_id_documents": official_docs,
            "members": [asdict(m) for m in members_sorted],
            "documents": [asdict(d) for d in documents_sorted],
            "data_id_brand_hints": [asdict(h) for h in data_id_hints_sorted],
            "data_id_brand_hints_index": hints_by_data_id,
            "product_pages": self.product_pages,
            "pages": [asdict(p) for p in self.pages],
            "errors": self.errors,
        }


def parse_args(argv: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Crawl publieke OpenTherm.eu-pagina's en maak JSON-output.")
    parser.add_argument("--base-url", default=BASE_URL, help="Basis-URL, standaard: https://www.opentherm.eu/")
    parser.add_argument("--output", default="opentherm_public.json", help="Pad van JSON-outputbestand")
    parser.add_argument("--max-pages", type=int, default=400, help="Maximaal aantal pagina's om te crawlen")
    parser.add_argument("--delay", type=float, default=0.75, help="Delay tussen requests in seconden")
    parser.add_argument("--timeout", type=float, default=20.0, help="HTTP-timeout in seconden")
    parser.add_argument("--verbose", action="store_true", help="Meer logging")
    return parser.parse_args(argv)


def main(argv: List[str]) -> int:
    args = parse_args(argv)

    crawler = OpenThermCrawler(
        base_url=args.base_url,
        delay=args.delay,
        timeout=args.timeout,
        max_pages=args.max_pages,
        verbose=args.verbose,
    )
    crawler.crawl()
    payload = crawler.build_json()

    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2, ensure_ascii=False)

    logging.info("JSON geschreven naar %s", args.output)
    logging.info("Stats: %s", json.dumps(payload["meta"]["stats"], ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
