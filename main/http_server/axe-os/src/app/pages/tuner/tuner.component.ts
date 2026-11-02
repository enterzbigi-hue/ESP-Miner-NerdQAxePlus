import { Component, OnDestroy, OnInit } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { HttpClient, HttpHeaders } from '@angular/common/http';
import { NbButtonModule, NbCardModule, NbInputModule } from '@nebular/theme';
import { TranslateModule, TranslateService } from '@ngx-translate/core';
import { EMPTY, Subject, catchError, exhaustMap, finalize, switchMap, takeUntil, timer } from 'rxjs';
import { SystemService } from '../../services/system.service';
import { ISettingsV2Pool } from '../../models/ISettingsV2';
import { OtpAuthService } from '../../services/otp-auth.service';

interface Hit { diff: number; hex: string; }
interface Step {
  index: number; upper_pct: number; lower_pct: number; low_hex: string; high_hex: string;
  completed: boolean; blocks: number; samples: number; jobs: number; seconds: number;
  empty?: boolean; max_diff: number; is_best: boolean; exhausted: boolean; top: Hit[];
}
interface TunerState {
  enabled: boolean; finished: boolean; current_step: number; best_step: number;
  start_pct: number; manual_pct: number; step_count: number; blocks_target: number;
  candidate_height: number; extranonce_bytes: number; current_hex: string;
  persistence_ok: boolean; steps: Step[];
  search_bits: number; effective_bits: number; required_protocol: number; session_protocol: number; compatible: boolean;
}

@Component({
  selector: 'app-tuner',
  standalone: true,
  imports: [CommonModule, FormsModule, NbButtonModule, NbCardModule, NbInputModule, TranslateModule],
  templateUrl: './tuner.component.html',
  styleUrls: ['./tuner.component.scss'],
})
export class TunerComponent implements OnInit, OnDestroy {
  state?: TunerState;
  startPct: number | string = 100;
  manualPct: number | string = 73.24;
  searchBits = 32;
  requiredProtocol = 0;
  widthOptions = [
    { value: 32, label: 'TUNER.W32' },
    { value: 0, label: 'TUNER.WAUTO' },
    { value: 4, label: 'TUNER.W4' },
    { value: 8, label: 'TUNER.W8' },
    { value: 16, label: 'TUNER.W16' },
    { value: 64, label: 'TUNER.W64' },
    { value: 128, label: 'TUNER.W128' },
    { value: 256, label: 'TUNER.W256' },
  ];
  poolIndex = 0;
  connectionMode = 1;
  connectionHost = '';
  connectionPort = 3333;
  connectionKey = '';
  connectionReady = false;
  connectionInfo = '';
  private pools: ISettingsV2Pool[] = [];
  busy = false;
  error = '';
  private loaded = false;
  private destroy$ = new Subject<void>();
  constructor(private http: HttpClient, private otp: OtpAuthService, private system: SystemService,
              private translate: TranslateService) {}
  private t(key: string, params?: object): string { return this.translate.instant('TUNER.' + key, params); }
  get current(): Step | undefined { return this.state?.steps[this.state.current_step]; }
  get progress(): number {
    if (!this.state) return 0;
    return 100 * this.state.steps.reduce((sum, row) => sum + Math.min(10, row.blocks), 0) / (10 * this.state.step_count);
  }
  get validStart(): boolean {
    const value = Number(String(this.startPct).replace(',', '.'));
    return Number.isFinite(value) && value >= 0.01 && value <= 100;
  }
  get validManual(): boolean {
    const value = Number(String(this.manualPct).replace(',', '.'));
    return String(this.manualPct).trim() !== '' && Number.isFinite(value) && value >= 0 && value <= 100;
  }
  get best(): Step | undefined { return this.state?.steps.find(row => row.is_best); }
  protocolName(value: number): string {
    return ({1: 'Stratum V1', 2: 'Stratum V2 Extended', 3: 'Stratum V2 Standard'} as Record<number,string>)[value] || this.t('WAITING_SESSION');
  }
  get validConnection(): boolean {
    return this.connectionReady && !!this.connectionHost.trim() && Number.isInteger(this.connectionPort) &&
      this.connectionPort > 0 && this.connectionPort <= 65535;
  }
  selectPool(index: number): void {
    this.poolIndex = index;
    const pool = this.pools[index];
    if (!pool) { this.connectionReady = false; return; }
    this.connectionMode = pool.protocol === 0 ? 1 : pool.sv2ChannelType === 1 ? 3 : 2;
    this.connectionHost = pool.url;
    this.connectionPort = pool.port;
    this.connectionKey = pool.sv2AuthorityPubkey || '';
    this.connectionReady = true;
  }
  saveConnection(): void {
    if (this.busy || !this.validConnection) return;
    this.busy = true; this.connectionInfo = '';
    const index = this.poolIndex;
    const patch = {
      url: this.connectionHost.trim(), port: this.connectionPort,
      protocol: this.connectionMode === 1 ? 0 : 1,
      sv2ChannelType: this.connectionMode === 3 ? 1 : 0,
      sv2AuthorityPubkey: this.connectionKey.trim(),
    };
    this.otp.ensureOtp$('', this.t('OTP_POOL_TITLE'), this.t('OTP_POOL_TEXT')).pipe(
      switchMap(({totp}) => this.system.updateSettingsV2('', {pools: index === 0 ? [patch] : [{}, patch]}, totp)),
      takeUntil(this.destroy$), finalize(() => this.busy = false),
    ).subscribe({
      next: () => {
        this.pools[index] = {...this.pools[index], ...patch};
        this.connectionInfo = this.t('CONN_SAVED');
      },
      error: () => this.connectionInfo = this.t('CONN_ERR'),
    });
  }
  ngOnInit(): void {
    this.system.getSettingsV2().pipe(takeUntil(this.destroy$)).subscribe({
      next: settings => { this.pools = settings.pools || []; this.selectPool(0); },
      error: () => this.connectionInfo = this.t('SETTINGS_ERR'),
    });
    timer(0, 5000).pipe(
      exhaustMap(() => this.http.get<TunerState>('/api/v2/tuner').pipe(
        catchError(() => { this.error = this.t('STATE_ERR'); return EMPTY; }),
      )),
      takeUntil(this.destroy$),
    ).subscribe(state => {
      this.state = state;
      this.error = '';
      if (!this.loaded) {
        this.startPct = state.start_pct;
        this.manualPct = state.manual_pct;
        this.searchBits = state.search_bits ?? 32;
        this.requiredProtocol = state.required_protocol ?? 0;
        this.loaded = true;
      }
    });
  }
  command(action: 'start' | 'pause' | 'reset'): void {
    if (this.busy || (action === 'reset' && !this.validStart) || (action === 'pause' && !this.validManual)) return;
    this.busy = true;
    this.error = '';
    this.otp.ensureOtp$('', this.t('TITLE'), this.t('OTP_TUNER_TEXT')).pipe(
      switchMap(({ totp }) => {
        const headers = totp ? new HttpHeaders({ 'X-TOTP': totp }) : undefined;
        const payload = action === 'reset'
          ? { action, start_pct: Number(String(this.startPct).replace(',', '.')), search_bits: this.searchBits, required_protocol: this.requiredProtocol } : action === 'pause' ? {action, manual_pct: Number(String(this.manualPct).replace(',', '.'))} : { action };
        return this.http.post<TunerState>('/api/v2/tuner', payload, { headers });
      }),
      takeUntil(this.destroy$),
      finalize(() => this.busy = false),
    ).subscribe({
      next: state => this.state = state,
      error: () => this.error = this.t('CMD_ERR'),
    });
  }
  formatDuration(value: number): string {
    if (!Number.isFinite(value) || value < 0) return '—';
    const seconds = Math.floor(value);
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const remaining = seconds % 60;
    const s = String(remaining).padStart(2, '0');
    return hours ? this.t('DURATION_H', { h: hours, m: minutes, s }) : this.t('DURATION', { m: minutes, s });
  }
  exportCsv(): void {
    if (!this.state) return;
    const rows: (string | number)[][] = [['upper_percent', 'lower_percent', 'blocks', 'jobs', 'samples', 'seconds', 'rank', 'difficulty', 'extranonce2']];
    this.state.steps.forEach(row => {
      (row.top.length ? row.top : [{ diff: 0, hex: '' }]).forEach((hit, index) =>
        rows.push([row.upper_pct, row.lower_pct, row.blocks, row.jobs, row.samples, row.seconds, index + 1, hit.diff, hit.hex]));
    });
    const csv = rows.map(row => row.map(cell => `"${String(cell).replace(/"/g, '""')}"`).join(',')).join('\r\n');
    const url = URL.createObjectURL(new Blob([csv], { type: 'text/csv;charset=utf-8' }));
    const link = document.createElement('a'); link.href = url; link.download = 'extranonce2-tuner.csv'; link.click();
    URL.revokeObjectURL(url);
  }
  ngOnDestroy(): void { this.destroy$.next(); this.destroy$.complete(); }
}
