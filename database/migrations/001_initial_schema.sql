-- ============================================================================
-- vamp9 Enterprise POS - Initial Database Schema (SQLite3 / WAL Mode)
-- Migration: 001_initial_schema.sql
-- Notes:
--   * Monetary amounts are stored as exact integer minor units (cents).
--   * Tax rates are stored as integer basis points (e.g., 2100 = 21.00%).
--   * Timestamps use ISO-8601 UTC text format (YYYY-MM-DDTHH:MM:SSZ).
-- ============================================================================

PRAGMA foreign_keys = ON;

-- ----------------------------------------------------------------------------
-- 1. Products Catalog
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS products (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    sku                 TEXT    NOT NULL UNIQUE,
    barcode             TEXT    UNIQUE,
    name                TEXT    NOT NULL,
    category            TEXT    NOT NULL DEFAULT 'General',
    unit_price_cents    INTEGER NOT NULL CHECK (unit_price_cents >= 0),
    tax_rate_bps        INTEGER NOT NULL DEFAULT 0 CHECK (tax_rate_bps >= 0 AND tax_rate_bps <= 10000),
    stock_quantity      INTEGER NOT NULL DEFAULT 0,
    is_active           INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0, 1)),
    created_at          TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    updated_at          TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
);

CREATE INDEX IF NOT EXISTS idx_products_barcode ON products(barcode);
CREATE INDEX IF NOT EXISTS idx_products_name    ON products(name);
CREATE INDEX IF NOT EXISTS idx_products_active  ON products(is_active);

-- ----------------------------------------------------------------------------
-- 2. Cash Registers (Shifts / Drawer Sessions)
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS cash_registers (
    id                      INTEGER PRIMARY KEY AUTOINCREMENT,
    register_code           TEXT    NOT NULL,
    cashier_name            TEXT    NOT NULL,
    opening_balance_cents   INTEGER NOT NULL DEFAULT 0 CHECK (opening_balance_cents >= 0),
    closing_balance_cents   INTEGER,
    expected_balance_cents  INTEGER,
    status                  TEXT    NOT NULL DEFAULT 'OPEN'
                                    CHECK (status IN ('OPEN', 'CLOSED', 'SUSPENDED')),
    opened_at               TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    closed_at               TEXT
);

CREATE INDEX IF NOT EXISTS idx_cash_registers_status ON cash_registers(status);

-- ----------------------------------------------------------------------------
-- 3. Sales (Header / Receipts)
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS sales (
    id                      INTEGER PRIMARY KEY AUTOINCREMENT,
    receipt_number          TEXT    NOT NULL UNIQUE,
    cash_register_id        INTEGER,
    subtotal_cents          INTEGER NOT NULL DEFAULT 0 CHECK (subtotal_cents >= 0),
    tax_total_cents         INTEGER NOT NULL DEFAULT 0 CHECK (tax_total_cents >= 0),
    discount_total_cents    INTEGER NOT NULL DEFAULT 0 CHECK (discount_total_cents >= 0),
    grand_total_cents       INTEGER NOT NULL DEFAULT 0 CHECK (grand_total_cents >= 0),
    payment_method          TEXT    NOT NULL DEFAULT 'CASH'
                                    CHECK (payment_method IN ('CASH', 'CARD', 'TRANSFER', 'MIXED')),
    status                  TEXT    NOT NULL DEFAULT 'COMPLETED'
                                    CHECK (status IN ('OPEN', 'COMPLETED', 'VOIDED', 'REFUNDED')),
    sync_status             TEXT    NOT NULL DEFAULT 'PENDING'
                                    CHECK (sync_status IN ('PENDING', 'SYNCED', 'FAILED')),
    created_at              TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    FOREIGN KEY (cash_register_id) REFERENCES cash_registers(id) ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_sales_created_at  ON sales(created_at);
CREATE INDEX IF NOT EXISTS idx_sales_sync_status ON sales(sync_status);

-- ----------------------------------------------------------------------------
-- 4. Sale Items (Immutable Line Snapshots)
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS sale_items (
    id                      INTEGER PRIMARY KEY AUTOINCREMENT,
    sale_id                 INTEGER NOT NULL,
    product_id              INTEGER,
    sku_snapshot            TEXT    NOT NULL,
    product_name_snapshot   TEXT    NOT NULL,
    quantity                INTEGER NOT NULL CHECK (quantity > 0),
    unit_price_cents        INTEGER NOT NULL CHECK (unit_price_cents >= 0),
    tax_rate_bps            INTEGER NOT NULL DEFAULT 0 CHECK (tax_rate_bps >= 0 AND tax_rate_bps <= 10000),
    tax_amount_cents        INTEGER NOT NULL DEFAULT 0 CHECK (tax_amount_cents >= 0),
    line_total_cents        INTEGER NOT NULL CHECK (line_total_cents >= 0),
    FOREIGN KEY (sale_id)    REFERENCES sales(id)    ON DELETE CASCADE,
    FOREIGN KEY (product_id) REFERENCES products(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_sale_items_sale_id    ON sale_items(sale_id);
CREATE INDEX IF NOT EXISTS idx_sale_items_product_id ON sale_items(product_id);

-- ----------------------------------------------------------------------------
-- 5. Offline-First Outbox Synchronization Queue
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS sync_queue (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    aggregate_type      TEXT    NOT NULL,
    aggregate_id        TEXT    NOT NULL,
    event_type          TEXT    NOT NULL,
    payload_json        TEXT    NOT NULL,
    status              TEXT    NOT NULL DEFAULT 'PENDING'
                                CHECK (status IN ('PENDING', 'IN_PROGRESS', 'SYNCED', 'FAILED', 'DEAD_LETTER')),
    retry_count         INTEGER NOT NULL DEFAULT 0 CHECK (retry_count >= 0),
    max_retries         INTEGER NOT NULL DEFAULT 10 CHECK (max_retries > 0),
    last_error          TEXT,
    next_attempt_at     TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    created_at          TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    processed_at        TEXT
);

CREATE INDEX IF NOT EXISTS idx_sync_queue_status_next ON sync_queue(status, next_attempt_at);
