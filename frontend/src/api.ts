export interface QueryResult {
  status: 'success' | 'error';
  message?: string;
  columns: string[];
  data: string[][];
  execution_time_ms: number;
  generated_sql?: string;
}

export interface Metrics {
  qps: number;
  active_connections: number;
  buffer_pool_hit_rate: number;
  active_transactions: number;
  total_queries: number;
  uptime_seconds: number;
  timestamp?: number;
}

export interface MetricsHistoryPoint {
  time: string;
  qps: number;
  buffer_pool_hit_rate: number;
  active_connections: number;
  active_transactions: number;
}

export interface LockGraph {
  has_deadlock: boolean;
  nodes: { id: string; label: string; status: string }[];
  edges: { from: string; to: string; label: string }[];
  cycle_node_ids: string[];
}

export interface ExplainNode {
  id: string;
  label: string;
  type: string;
  cost: number;
  rows: number;
  children?: ExplainNode[];
}

export interface ExplainResult {
  status: 'success' | 'error';
  message?: string;
  plan_tree?: ExplainNode;
  raw?: string;
  simulated?: boolean;
}

export interface ColumnInfo {
  name: string;
  type: string;
  nullable?: boolean;
  default?: string | null;
  key?: string;
}

export interface TableInfo {
  name: string;
  columns: ColumnInfo[];
}

export interface SchemaTree {
  tables: TableInfo[];
  error?: string;
}

const BASE = '/api';

async function request<T>(url: string, options?: RequestInit): Promise<T> {
  const isFormData = options?.body instanceof FormData;
  const res = await fetch(`${BASE}${url}`, {
    ...options,
    headers: {
      ...(isFormData ? {} : { 'Content-Type': 'application/json' }),
      ...(options?.headers as Record<string, string>),
    },
  });
  if (!res.ok) {
    throw new Error(`API error: ${res.status} ${res.statusText}`);
  }
  return res.json();
}

export function executeQuery(sql: string): Promise<QueryResult> {
  return request<QueryResult>('/query', {
    method: 'POST',
    body: JSON.stringify({ sql }),
  });
}

export function chatQuery(message: string, history: { role: string; content: string }[] = []): Promise<QueryResult> {
  return request<QueryResult>('/chat', {
    method: 'POST',
    body: JSON.stringify({ message, history }),
  });
}

export function fetchMetrics(): Promise<Metrics> {
  return request<Metrics>('/metrics');
}

export function fetchMetricsHistory(): Promise<MetricsHistoryPoint[]> {
  return request<MetricsHistoryPoint[]>('/metrics/history');
}

export function fetchLockGraph(): Promise<LockGraph> {
  return request<LockGraph>('/lock-graph');
}

export function fetchExplainPlan(sql: string): Promise<ExplainResult> {
  return request<ExplainResult>('/explain', {
    method: 'POST',
    body: JSON.stringify({ sql }),
  });
}

export interface ExplainAIResult {
  status: 'success' | 'error' | 'not_configured';
  explanation?: string;
  message?: string;
}

export function fetchExplainAI(sql: string, planRaw: string, simulated: boolean): Promise<ExplainAIResult> {
  return request<ExplainAIResult>('/explain/ai', {
    method: 'POST',
    body: JSON.stringify({ sql, plan_raw: planRaw, simulated }),
  });
}

export function fetchSchemaTree(): Promise<SchemaTree> {
  return request<SchemaTree>('/schema/tree');
}

// ── Workspace (Dashboard sidebar tree) ──────────────────────────────────────

export interface WorkspaceFolder {
  id: number;
  name: string;
  parent_id: number | null;
  sort_order: number;
}

export interface WorkspaceTable {
  id: number;
  table_name: string;
  display_name: string;
  folder_id: number | null;
  columns?: { name: string; type: string; pk?: boolean; auto_increment?: boolean }[];
  columns_json?: string;
  exists_in_rmdb?: boolean;
  sort_order: number;
}

export interface WorkspaceTree {
  status: string;
  folders: WorkspaceFolder[];
  tables: WorkspaceTable[];
}

export function fetchWorkspaceTree(): Promise<WorkspaceTree> {
  return request<WorkspaceTree>('/workspace/tree', { headers: authHeader() });
}

export function createWorkspaceFolder(name: string, parentId?: number | null): Promise<{ status: string; folder: WorkspaceFolder }> {
  return request<{ status: string; folder: WorkspaceFolder }>('/workspace/folders', {
    method: 'POST', headers: authHeader(), body: JSON.stringify({ name, parent_id: parentId ?? null }),
  });
}

export function renameWorkspaceFolder(folderId: number, name: string): Promise<{ status: string }> {
  return request('/workspace/folders/' + folderId, {
    method: 'PUT', headers: authHeader(), body: JSON.stringify({ name }),
  });
}

export function deleteWorkspaceFolder(folderId: number): Promise<{ status: string }> {
  return request('/workspace/folders/' + folderId, { method: 'DELETE', headers: authHeader() });
}

export function createWorkspaceTable(name: string, folderId?: number | null): Promise<{ status: string; table: WorkspaceTable }> {
  return request<{ status: string; table: WorkspaceTable }>('/workspace/tables', {
    method: 'POST', headers: authHeader(), body: JSON.stringify({ name, folder_id: folderId ?? null }),
  });
}

export function renameWorkspaceTable(tableId: number, name: string): Promise<{ status: string }> {
  return request('/workspace/tables/' + tableId + '/rename', {
    method: 'PUT', headers: authHeader(), body: JSON.stringify({ name }),
  });
}

export function moveWorkspaceTable(tableId: number, folderId: number | null): Promise<{ status: string }> {
  return request('/workspace/tables/' + tableId + '/move', {
    method: 'PUT', headers: authHeader(), body: JSON.stringify({ folder_id: folderId }),
  });
}

export function deleteWorkspaceTable(tableId: number): Promise<{ status: string }> {
  return request('/workspace/tables/' + tableId, { method: 'DELETE', headers: authHeader() });
}

export function dropTable(tableName: string): Promise<{ status: string; message?: string }> {
  return request(`/schema/tables/${encodeURIComponent(tableName)}`, { method: 'DELETE' });
}

export interface HistoryEntry {
  time: string;
  sql: string;
  elapsed_ms: number;
  status: string;
  rows: number;
}

export function fetchQueryHistory(limit?: number): Promise<HistoryEntry[]> {
  const qs = limit ? `?limit=${limit}` : '';
  return request<HistoryEntry[]>(`/history${qs}`);
}

// ── Scenarios ──────────────────────────────────────────────────────────────

export interface Scenario {
  id: number;
  name: string;
  slug: string;
  description: string;
  icon: string;
  is_active: number;
  created_at: string;
}

export interface ScenarioTable {
  id: number;
  scenario_id: number;
  table_name: string;
  display_name: string;
  description: string;
  columns?: ColumnInfo[];
  row_count?: number;
}

export interface ScenarioDetail {
  status: string;
  scenario: Scenario;
  tables: ScenarioTable[];
}

export function fetchScenarios(): Promise<{ status: string; scenarios: Scenario[] }> {
  return request('/scenarios', { headers: authHeader() });
}

export function createScenario(name: string, description: string, icon: string, slug?: string): Promise<{ status: string; scenario: Scenario }> {
  return request('/scenarios', {
    method: 'POST',
    headers: authHeader(),
    body: JSON.stringify({ name, description, icon, slug: slug || '' }),
  });
}

export function getScenario(id: number): Promise<ScenarioDetail> {
  return request(`/scenarios/${id}`, { headers: authHeader() });
}

export function deleteScenario(id: number): Promise<{ status: string; message: string }> {
  return request(`/scenarios/${id}`, { method: 'DELETE', headers: authHeader() });
}

export function activateScenario(id: number): Promise<{ status: string; message: string }> {
  return request(`/scenarios/${id}/activate`, { method: 'POST', headers: authHeader() });
}

export function createScenarioTable(scenarioId: number, displayName: string, description: string, columns: { name: string; type: string; pk?: boolean; not_null?: boolean; auto_increment?: boolean; default_value?: string }[]): Promise<{ status: string; table: { id: number; table_name: string; display_name: string } }> {
  return request(`/scenarios/${scenarioId}/tables`, {
    method: 'POST',
    headers: authHeader(),
    body: JSON.stringify({ display_name: displayName, description, columns }),
  });
}

export function deleteScenarioTable(scenarioId: number, tableId: number): Promise<{ status: string; message: string }> {
  return request(`/scenarios/${scenarioId}/tables/${tableId}`, { method: 'DELETE', headers: authHeader() });
}

// ── Table Structure Modification (Simulated ALTER TABLE) ──────────────────────

export function modifyTableStructure(
  scenarioId: number,
  tableId: number,
  columns: { name: string; type: string; pk?: boolean; not_null?: boolean; auto_increment?: boolean; default_value?: string }[],
): Promise<{ status: string; message?: string; table?: { id: number; table_name: string; display_name: string }; inserted?: number; errors?: string[] }> {
  return request(`/scenarios/${scenarioId}/tables/${tableId}/structure`, {
    method: 'PUT',
    headers: authHeader(),
    body: JSON.stringify({ columns }),
  });
}

// ── AI Schema Design ────────────────────────────────────────────────────────

export interface AIDesignResult {
  status: string;
  response?: string;
  message?: string;
}

export function fetchAIDesign(scenarioId: number, description: string, history: { role: string; content: string }[] = []): Promise<AIDesignResult> {
  return request(`/scenarios/${scenarioId}/ai-design`, {
    method: 'POST',
    headers: authHeader(),
    body: JSON.stringify({ description, history }),
  });
}

export function confirmAIDesign(scenarioId: number, sqls: string[]): Promise<{ status: string; created: { table_name: string; display_name: string }[]; errors: { table?: string; sql?: string; error: string }[] }> {
  return request(`/scenarios/${scenarioId}/ai-confirm`, {
    method: 'POST',
    headers: authHeader(),
    body: JSON.stringify({ sqls }),
  });
}

// ── Data CRUD ───────────────────────────────────────────────────────────────

export interface DataRowsResult {
  status: string;
  columns: string[];
  data: string[][];
  total: number;
  page: number;
  size: number;
  total_pages: number;
  message?: string;
}

export function fetchRows(tableName: string, page: number = 1, size: number = 50, search: string = '', sort: string = '', order: string = 'asc'): Promise<DataRowsResult> {
  const params = new URLSearchParams({ page: String(page), size: String(size), search, sort, order });
  return request(`/data/${tableName}/rows?${params}`, { headers: authHeader() });
}

export function insertRow(tableName: string, data: Record<string, string>): Promise<{ status: string; message?: string }> {
  return request(`/data/${tableName}/rows`, {
    method: 'POST',
    headers: authHeader(),
    body: JSON.stringify({ data }),
  });
}

export function updateRow(tableName: string, pkValue: string, data: Record<string, string>): Promise<{ status: string; message?: string }> {
  return request(`/data/${tableName}/rows/${encodeURIComponent(pkValue)}`, {
    method: 'PUT',
    headers: authHeader(),
    body: JSON.stringify({ data }),
  });
}

export function deleteRow(tableName: string, pkValue: string): Promise<{ status: string; message?: string }> {
  return request(`/data/${tableName}/rows/${encodeURIComponent(pkValue)}`, {
    method: 'DELETE',
    headers: authHeader(),
  });
}

export async function importCSV(tableName: string, file: File): Promise<{ status: string; imported: number; errors: { row: number; error: string }[] }> {
  const formData = new FormData();
  formData.append('file', file);
  const res = await fetch(`/api/data/${tableName}/import`, {
    method: 'POST',
    headers: { 'Authorization': `Bearer ${localStorage.getItem('aidv_token')}` },
    body: formData,
  });
  if (!res.ok) throw new Error(`Import failed: ${res.status}`);
  return res.json();
}

export function getExportURL(tableName: string): string {
  return `/api/data/${tableName}/export`;
}

export async function downloadExport(tableName: string): Promise<void> {
  const res = await fetch(`${BASE}/data/${tableName}/export`, {
    headers: authHeader(),
  });
  if (!res.ok) throw new Error(`Export failed: ${res.status}`);
  const blob = await res.blob();
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `${tableName}.csv`;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

// ── Auth Helper ─────────────────────────────────────────────────────────────

// ── SQL Import ──────────────────────────────────────────────────────────────

export interface ParsedSQL {
  status: string;
  creates: { table_name: string; col_count: number; sql: string }[];
  inserts: { table_name: string; row_count: number; sql: string }[];
  others: { type: string; sql: string }[];
  filename: string;
  message?: string;
}

export interface ImportResult {
  status: string;
  created: number;
  inserted: number;
  errors: { table: string; error: string; type: string }[];
}

export async function parseSQLFile(file: File): Promise<ParsedSQL> {
  const formData = new FormData();
  formData.append('file', file);
  const res = await fetch('/api/import/parse', { method: 'POST', body: formData });
  if (!res.ok) throw new Error(`Parse failed: ${res.status}`);
  return res.json();
}

export function executeImport(creates: string[], inserts: string[]): Promise<ImportResult> {
  return request('/import/execute', {
    method: 'POST',
    body: JSON.stringify({ creates, inserts }),
  });
}

// ── Admin ────────────────────────────────────────────────────────────────────

export interface AdminUser {
  id: number;
  username: string;
  email: string;
  is_admin: number;
  created_at: string;
}

export function fetchAdminUsers(): Promise<{ status: string; users: AdminUser[]; message?: string }> {
  return request('/admin/users', { headers: authHeader() });
}

export function deleteAdminUser(userId: number): Promise<{ status: string; message?: string }> {
  return request('/admin/users/' + userId, { method: 'DELETE', headers: authHeader() });
}

export function toggleAdminUser(userId: number, isAdmin: boolean): Promise<{ status: string; message?: string }> {
  return request('/admin/users/' + userId + '/admin', {
    method: 'PUT',
    headers: authHeader(),
    body: JSON.stringify({ is_admin: isAdmin }),
  });
}

function authHeader(): Record<string, string> {
  const token = localStorage.getItem('aidv_token');
  return token ? { 'Authorization': `Bearer ${token}` } : {};
}
