//! ratatui 单屏 TUI：左侧选择操作，右侧配置参数，返回完整的 [`Cli`]。
//! 退出方式：q / Esc / Ctrl+C。raw 模式下 Ctrl+C 以按键事件到达而非信号，需手动处理。

use std::{
    fs,
    path::{Path, PathBuf},
};

use anyhow::Result;
use crossterm::event::{self, Event, KeyCode, KeyEvent, KeyEventKind, KeyModifiers};
use once_fn::once;
use ratatui::{
    DefaultTerminal, Frame,
    layout::{Constraint, Layout, Margin, Rect},
    style::{Color, Style, Stylize},
    text::{Line, Span},
    widgets::{Block, Cell, List, ListState, Paragraph, Row, Table, TableState, Wrap},
};

use crate::{
    cache::GLOBAL_CACHE,
    cli::{Cli, Commands, DetectArgs, UnpackDllArgs},
    utils::{SPEED_MAX, SupportedDLLs},
};

const NONE_EXEC_ITEM: &str = "None";
/// [`ARCH_OPTIONS`] 中 x86 的下标
const ARCH_X86_IDX: usize = 1;
/// 无上次命令时的默认速度
const DEFAULT_SPEED: f32 = 2.0;

/// DLL 选项：(显示名, CLI 值)
const DLL_OPTIONS: [(&str, SupportedDLLs); 3] = [
    ("MMDevAPI", SupportedDLLs::MMDevAPI),
    ("dsound", SupportedDLLs::DSound),
    ("ALL", SupportedDLLs::ALL),
];

/// "Auto/x64" 即不指定 x86，配合 exec 检测时为自动
const ARCH_OPTIONS: [&str; 2] = ["Auto/x64", "x86"];

/// 参数行类型，决定取值与取值范围
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Param {
    Dll,
    Arch,
    Speed,
    Exec,
}

impl Param {
    fn label(self) -> &'static str {
        match self {
            Param::Dll => "DLL",
            Param::Arch => "架构",
            Param::Speed => "速度",
            Param::Exec => "游戏 exe",
        }
    }

    /// 取值数量，用于循环切换
    fn options_len(self) -> usize {
        match self {
            Param::Dll => DLL_OPTIONS.len(),
            Param::Arch => ARCH_OPTIONS.len(),
            Param::Speed => speed_options().len(),
            Param::Exec => exec_options().len(),
        }
    }
}

/// 操作类型
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Action {
    Speedup,
    ZeroInterrupt,
    Detect,
    Clean,
    Exit,
}

/// 全部操作（固定顺序，数字键 1-5 对应下标 0-4）
const ACTIONS: [Action; 5] = [
    Action::Speedup,
    Action::ZeroInterrupt,
    Action::Detect,
    Action::Clean,
    Action::Exit,
];

impl Action {
    fn label(self) -> &'static str {
        match self {
            Action::Speedup => "语音加速 (SPEEDUP)",
            Action::ZeroInterrupt => "语音不中断 (ZeroInterrupt)",
            Action::Detect => "检测架构 (Detect)",
            Action::Clean => "清除残留 (Clean)",
            Action::Exit => "退出 (Exit)",
        }
    }

    /// 该操作需要配置的参数行
    fn params(self) -> &'static [Param] {
        match self {
            Action::Speedup => &[Param::Dll, Param::Arch, Param::Speed, Param::Exec],
            Action::ZeroInterrupt => &[Param::Arch, Param::Exec],
            Action::Detect => &[Param::Exec],
            Action::Clean | Action::Exit => &[],
        }
    }

    /// 无参数操作在右侧面板展示的说明
    fn description(self) -> &'static str {
        match self {
            Action::Clean => {
                "还原所有 AudioSpeedHack 所做的更改，包括注册表项、DLL 文件和环境变量。"
            }
            Action::Exit => "退出程序。",
            _ => "",
        }
    }
}

/// 当前焦点面板
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Focus {
    Actions,
    Params,
}

/// 各参数当前选中的下标
#[derive(Debug)]
struct ParamValues {
    dll: usize,
    arch: usize,
    speed: usize,
    exec: usize,
}

impl Default for ParamValues {
    fn default() -> Self {
        Self {
            dll: 0,
            arch: 0,
            speed: speed_idx(DEFAULT_SPEED),
            exec: 0,
        }
    }
}

/// 单次按键的处理结果
enum Outcome {
    Running,
    Quit,
    Run(Cli),
}

struct App {
    action_idx: usize,
    param_idx: usize,
    focus: Focus,
    values: ParamValues,
    /// 面板内展示的错误信息（如 Detect 未选 exe），任意导航操作清除
    status: Option<String>,
}

/// TUI 主函数。返回 None 表示用户主动退出（q / Esc / Ctrl+C / Exit）
pub fn run_tui() -> Result<Option<Cli>> {
    // 先读缓存再初始化终端，避免日志输出破坏终端状态
    let prev_cli = GLOBAL_CACHE.lock().unwrap().last_command.clone();
    let mut app = App::new(prev_cli.as_ref());

    let mut terminal = ratatui::init();
    let result = app.run(&mut terminal);
    ratatui::restore();
    result
}

impl App {
    /// 上次命令作为各参数的默认值载入，回车即可直接复跑
    fn new(prev: Option<&Commands>) -> Self {
        let (action_idx, values) = match prev {
            Some(Commands::UnpackDll(args)) if args.dll == SupportedDLLs::DSoundZeroInterrupt => (
                action_idx_of(Action::ZeroInterrupt),
                ParamValues {
                    arch: arch_idx(args.x86),
                    exec: exec_idx(args.exec.as_deref()),
                    ..ParamValues::default()
                },
            ),
            Some(Commands::UnpackDll(args)) => (
                action_idx_of(Action::Speedup),
                ParamValues {
                    dll: DLL_OPTIONS
                        .iter()
                        .position(|(_, d)| *d == args.dll)
                        .unwrap_or(0),
                    arch: arch_idx(args.x86),
                    speed: speed_idx(args.speed.unwrap_or(DEFAULT_SPEED)),
                    exec: exec_idx(args.exec.as_deref()),
                },
            ),
            Some(Commands::Detect(d)) => (
                action_idx_of(Action::Detect),
                ParamValues {
                    exec: exec_idx(Some(&d.exe)),
                    ..ParamValues::default()
                },
            ),
            Some(Commands::Clean) | None => (0, ParamValues::default()),
        };
        Self {
            action_idx,
            param_idx: 0,
            focus: Focus::Actions,
            values,
            status: None,
        }
    }

    fn action(&self) -> Action {
        ACTIONS[self.action_idx]
    }

    fn params(&self) -> &'static [Param] {
        self.action().params()
    }

    fn run(&mut self, terminal: &mut DefaultTerminal) -> Result<Option<Cli>> {
        loop {
            terminal.draw(|f| self.render(f))?;
            match self.handle_event()? {
                Outcome::Running => {}
                Outcome::Quit => return Ok(None),
                Outcome::Run(cli) => return Ok(Some(cli)),
            }
        }
    }

    fn handle_event(&mut self) -> Result<Outcome> {
        let Event::Key(key) = event::read()? else {
            return Ok(Outcome::Running);
        };
        // Windows 上按键会同时上报 Press/Release，只处理 Press
        if key.kind != KeyEventKind::Press {
            return Ok(Outcome::Running);
        }
        Ok(self.on_key(key))
    }

    fn on_key(&mut self, key: KeyEvent) -> Outcome {
        if key.code == KeyCode::Char('c') && key.modifiers.contains(KeyModifiers::CONTROL) {
            return Outcome::Quit;
        }
        match key.code {
            KeyCode::Char('q') | KeyCode::Esc => Outcome::Quit,
            KeyCode::Tab | KeyCode::BackTab => {
                self.toggle_focus();
                Outcome::Running
            }
            KeyCode::Up | KeyCode::Char('k') => {
                self.move_selection(false);
                Outcome::Running
            }
            KeyCode::Down | KeyCode::Char('j') => {
                self.move_selection(true);
                Outcome::Running
            }
            KeyCode::Left | KeyCode::Char('h') => {
                self.adjust(false);
                Outcome::Running
            }
            KeyCode::Right | KeyCode::Char('l') => {
                self.adjust(true);
                Outcome::Running
            }
            KeyCode::Enter => self.build_cli(),
            KeyCode::Char(c) => {
                if let Some(i) = c.to_digit(10).filter(|d| (1..=9).contains(d)) {
                    self.select_action(i as usize - 1);
                }
                Outcome::Running
            }
            _ => Outcome::Running,
        }
    }

    /// 切换操作后重置参数行焦点；新操作无参数时焦点回到操作面板
    fn after_action_change(&mut self) {
        self.param_idx = 0;
        self.status = None;
        if self.params().is_empty() {
            self.focus = Focus::Actions;
        }
    }

    fn move_selection(&mut self, down: bool) {
        match self.focus {
            Focus::Actions => {
                self.action_idx = wrapping_next(self.action_idx, ACTIONS.len(), down);
                self.after_action_change();
            }
            Focus::Params => {
                self.param_idx = wrapping_next(self.param_idx, self.params().len(), down);
                self.status = None;
            }
        }
    }

    fn select_action(&mut self, i: usize) {
        if i < ACTIONS.len() {
            self.action_idx = i;
            self.after_action_change();
            self.focus = Focus::Actions;
        }
    }

    fn toggle_focus(&mut self) {
        if self.params().is_empty() {
            self.focus = Focus::Actions;
        } else {
            self.focus = match self.focus {
                Focus::Actions => Focus::Params,
                Focus::Params => Focus::Actions,
            };
            self.status = None;
        }
    }

    /// 焦点在操作面板时，向右进入参数面板；在参数面板时增减当前值
    fn adjust(&mut self, forward: bool) {
        if self.focus == Focus::Actions {
            if forward && !self.params().is_empty() {
                self.focus = Focus::Params;
                self.status = None;
            }
            return;
        }
        let p = self.params()[self.param_idx];
        let len = p.options_len();
        let idx = match p {
            Param::Dll => &mut self.values.dll,
            Param::Arch => &mut self.values.arch,
            Param::Speed => &mut self.values.speed,
            Param::Exec => &mut self.values.exec,
        };
        *idx = wrapping_next(*idx, len, forward);
        self.status = None;
    }

    fn param_value(&self, p: Param) -> String {
        match p {
            Param::Dll => DLL_OPTIONS[self.values.dll].0.to_string(),
            Param::Arch => ARCH_OPTIONS[self.values.arch].to_string(),
            Param::Speed => speed_options()[self.values.speed].clone(),
            Param::Exec => exec_options()[self.values.exec].clone(),
        }
    }

    /// 将当前 UI 状态映射为 Cli；参数不满足时写入 status 并继续运行
    fn build_cli(&mut self) -> Outcome {
        let cmd = match self.action() {
            Action::Speedup => Commands::UnpackDll(UnpackDllArgs {
                dll: DLL_OPTIONS[self.values.dll].1,
                x86: self.values.arch == ARCH_X86_IDX,
                speed: Some(speed_value(self.values.speed)),
                exec: exec_value(self.values.exec),
            }),
            Action::ZeroInterrupt => Commands::UnpackDll(UnpackDllArgs {
                dll: SupportedDLLs::DSoundZeroInterrupt,
                x86: self.values.arch == ARCH_X86_IDX,
                speed: None,
                exec: exec_value(self.values.exec),
            }),
            Action::Detect => {
                let Some(exe) = exec_value(self.values.exec) else {
                    self.status = Some("错误: 请先在参数面板选择游戏 exe".to_string());
                    self.focus = Focus::Params;
                    return Outcome::Running;
                };
                Commands::Detect(DetectArgs { exe })
            }
            Action::Clean => Commands::Clean,
            Action::Exit => return Outcome::Quit,
        };
        Outcome::Run(Cli { command: cmd })
    }

    fn render(&self, f: &mut Frame) {
        let title = Line::from(vec![
            Span::styled(env!("CARGO_PKG_NAME"), Style::new().bold()),
            Span::raw(format!(" v{} ", env!("CARGO_PKG_VERSION"))),
            Span::styled(env!("CARGO_PKG_REPOSITORY"), Style::new().dim()),
        ])
        .centered();
        let area = f.area();
        f.render_widget(Block::bordered().title(title), area);

        let inner = area.inner(Margin::new(1, 1));
        let [main, footer] =
            Layout::vertical([Constraint::Min(3), Constraint::Length(2)]).areas(inner);
        let [actions_area, params_area] =
            Layout::horizontal([Constraint::Length(38), Constraint::Min(30)])
                .spacing(1)
                .areas(main);
        let [status_area, help_area] =
            Layout::vertical([Constraint::Length(1), Constraint::Length(1)]).areas(footer);

        self.render_actions(f, actions_area);
        self.render_params(f, params_area);
        if let Some(msg) = &self.status {
            f.render_widget(Paragraph::new(msg.as_str()).red(), status_area);
        }
        render_help(f, help_area);
    }

    fn render_actions(&self, f: &mut Frame, area: Rect) {
        let focused = self.focus == Focus::Actions;
        let items = ACTIONS.iter().enumerate().map(|(i, a)| {
            Line::from(vec![
                Span::styled(format!("[{}] ", (i + 1) % 10), Style::new().dim()),
                Span::raw(a.label()),
            ])
        });
        let list = List::new(items)
            .block(
                Block::bordered()
                    .title(" 操作 ")
                    .border_style(focus_border(focused)),
            )
            .highlight_symbol("› ")
            .highlight_style(highlight_row(focused));
        let mut state = ListState::default().with_selected(Some(self.action_idx));
        f.render_stateful_widget(list, area, &mut state);
    }

    fn render_params(&self, f: &mut Frame, area: Rect) {
        let focused = self.focus == Focus::Params;
        let params = self.params();
        let block = |title: &'static str| {
            Block::bordered()
                .title(title)
                .border_style(focus_border(focused))
        };

        if params.is_empty() {
            f.render_widget(
                Paragraph::new(self.action().description())
                    .block(block(" 说明 "))
                    .wrap(Wrap { trim: false }),
                area,
            );
            return;
        }

        let rows = params.iter().enumerate().map(|(i, p)| {
            let value = if focused && i == self.param_idx {
                format!("‹ {} ›", self.param_value(*p))
            } else {
                self.param_value(*p)
            };
            Row::new([Cell::from(p.label()), Cell::from(value)])
        });
        let table = Table::new(rows, [Constraint::Length(12), Constraint::Min(8)])
            .block(block(" 参数 "))
            .highlight_symbol("› ")
            .row_highlight_style(highlight_row(focused));
        let mut state = TableState::default().with_selected(Some(self.param_idx));
        f.render_stateful_widget(table, area, &mut state);
    }
}

fn render_help(f: &mut Frame, area: Rect) {
    let key = |s: &str| Span::styled(s.to_owned(), Style::new().bold());
    let text = |s: &str| Span::styled(s.to_owned(), Style::new().dim());
    f.render_widget(
        Paragraph::new(Line::from(vec![
            key("↑↓ 移动"),
            text("  "),
            key("←→ 修改"),
            text("  "),
            key("Tab 切换面板"),
            text("  "),
            key("Enter 运行"),
            text("  "),
            key("数字键 选择操作"),
            text("  "),
            key("q/Esc/Ctrl+C 退出"),
        ])),
        area,
    );
}

/// 下标循环移动
fn wrapping_next(idx: usize, len: usize, forward: bool) -> usize {
    if len == 0 {
        return 0;
    }
    if forward {
        (idx + 1) % len
    } else {
        (idx + len - 1) % len
    }
}

fn focus_border(focused: bool) -> Style {
    if focused {
        Style::new().fg(Color::Yellow)
    } else {
        Style::new().dim()
    }
}

fn highlight_row(focused: bool) -> Style {
    if focused {
        Style::new().fg(Color::Yellow).bold()
    } else {
        Style::new().fg(Color::Yellow)
    }
}

/// 速度选项下标转数值
fn speed_value(idx: usize) -> f32 {
    speed_options()[idx]
        .parse()
        .expect("speed option is a valid f32 literal")
}

/// 速度数值转选项下标，取首个不小于该值的选项，越界收敛到末尾
fn speed_idx(v: f32) -> usize {
    let opts = speed_options();
    opts.iter()
        .position(|s| s.parse::<f32>().is_ok_and(|f| f >= v))
        .unwrap_or(opts.len() - 1)
}

/// x86 参数转 [`ARCH_OPTIONS`] 下标
fn arch_idx(x86: bool) -> usize {
    if x86 { ARCH_X86_IDX } else { 0 }
}

/// exe 路径转 [`exec_options`] 下标，不在选项中时回退 None
fn exec_idx(exe: Option<&Path>) -> usize {
    exe.and_then(|p| exec_options().iter().position(|o| Path::new(o) == p))
        .unwrap_or(0)
}

fn action_idx_of(action: Action) -> usize {
    ACTIONS
        .iter()
        .position(|a| *a == action)
        .expect("action exists in ACTIONS")
}

fn exec_value(idx: usize) -> Option<PathBuf> {
    (idx > 0).then(|| exec_options()[idx].clone().into())
}

/// 生成速度选项 (1.0 ~ 2.0)
fn speed_options() -> Vec<String> {
    // 从 10 迭代到 SPEED_MAX * 10 (例如 20)，用整数十分位构造，避免浮点格式化误差
    let start = 10;
    // SPEED_MAX 为整数值常量，x10 后无截断
    #[allow(clippy::cast_possible_truncation)]
    let end = (SPEED_MAX * 10.0) as i32;

    (start..=end)
        .map(|x| format!("{}.{}", x / 10, x % 10))
        .collect()
}

/// 获取当前目录下的 exe 文件作为 `exec` 的选项
#[once]
fn exec_options() -> Vec<String> {
    let mut options = vec![NONE_EXEC_ITEM.to_string()];
    if let Ok(entries) = fs::read_dir(".") {
        for entry in entries
            .flatten()
            .filter(|e| e.file_type().is_ok_and(|t| t.is_file()))
            .filter(|e| {
                // 过滤掉自身
                if let Some(name) = e.file_name().to_str()
                    && name.contains(env!("CARGO_PKG_NAME"))
                {
                    return false;
                }
                e.path()
                    .extension()
                    .and_then(|e| e.to_str())
                    .is_some_and(|ext| ext.eq_ignore_ascii_case("exe"))
            })
        {
            if let Some(name) = entry.file_name().to_str() {
                options.push(name.to_string());
            }
        }
    }
    options
}

#[cfg(test)]
mod tests {
    use super::*;
    use ratatui::{Terminal, backend::TestBackend};

    /// 将 buffer 转为等宽文本：宽字符的后续占位格（symbol 为空格且无 skip 标记）需按显示宽度跳过，
    /// 逻辑与 ratatui-core `Buffer` 的 `Debug` 实现一致
    fn render_text(app: &mut App, w: u16, h: u16) -> String {
        use unicode_width::UnicodeWidthStr;

        let mut terminal = Terminal::new(TestBackend::new(w, h)).unwrap();
        terminal.draw(|f| app.render(f)).unwrap();
        let mut out = String::new();
        let mut skip = 0usize;
        for (i, cell) in terminal.backend().buffer().content().iter().enumerate() {
            if i > 0 && i % w as usize == 0 {
                out.push('\n');
            }
            if skip == 0 {
                out.push_str(cell.symbol());
            }
            let width = cell.symbol().width();
            skip = skip.max(width).saturating_sub(1);
        }
        out
    }

    #[test]
    fn render_smoke() {
        let mut app = App::new(None);
        let text = render_text(&mut app, 100, 30);
        for s in [
            "语音加速 (SPEEDUP)",
            "语音不中断 (ZeroInterrupt)",
            "参数",
            "游戏 exe",
            "2.0",
            "Ctrl+C",
        ] {
            assert!(text.contains(s), "missing {s:?} in:\n{text}");
        }
        println!("\n{text}");
    }

    #[test]
    fn build_cli_mapping() {
        let mut app = App::new(None);
        // 默认: Speedup + MMDevAPI + Auto/x64 + 速度 2.0 + None exec
        let Outcome::Run(cli) = app.build_cli() else {
            panic!("should run");
        };
        let Commands::UnpackDll(args) = cli.command else {
            panic!("should be UnpackDll");
        };
        assert_eq!(args.dll, SupportedDLLs::MMDevAPI);
        assert!(!args.x86);
        assert_eq!(args.speed, Some(2.0));
        assert_eq!(args.exec, None);

        app.values.dll = 2;
        app.values.arch = ARCH_X86_IDX;
        app.values.speed = 5; // 1.5
        let Outcome::Run(cli) = app.build_cli() else {
            panic!("should run");
        };
        let Commands::UnpackDll(args) = cli.command else {
            panic!("should be UnpackDll");
        };
        assert_eq!(args.dll, SupportedDLLs::ALL);
        assert!(args.x86);
        assert_eq!(args.speed, Some(1.5));
    }

    #[test]
    fn seed_from_prev_command() {
        // Speedup 参数完整载入并回车即可复跑
        let mut app = App::new(Some(&Commands::UnpackDll(UnpackDllArgs {
            dll: SupportedDLLs::ALL,
            x86: true,
            speed: Some(1.5),
            exec: None,
        })));
        assert_eq!(app.action(), Action::Speedup);
        let Outcome::Run(cli) = app.build_cli() else {
            panic!("should run");
        };
        let Commands::UnpackDll(args) = cli.command else {
            panic!("should be UnpackDll");
        };
        assert_eq!(args.dll, SupportedDLLs::ALL);
        assert!(args.x86);
        assert_eq!(args.speed, Some(1.5));

        // ZeroInterrupt
        let app = App::new(Some(&Commands::UnpackDll(UnpackDllArgs {
            dll: SupportedDLLs::DSoundZeroInterrupt,
            x86: false,
            speed: None,
            exec: None,
        })));
        assert_eq!(app.action(), Action::ZeroInterrupt);

        // Detect；exe 不在当前目录选项中时回退 None
        let app = App::new(Some(&Commands::Detect(DetectArgs {
            exe: "nonexist.exe".into(),
        })));
        assert_eq!(app.action(), Action::Detect);
        assert_eq!(app.values.exec, 0);
    }

    #[test]
    fn detect_requires_exec() {
        let mut app = App::new(None);
        app.select_action(2); // Detect
        assert!(matches!(app.build_cli(), Outcome::Running));
        assert!(app.status.is_some());
    }

    #[test]
    fn exit_action() {
        let mut app = App::new(None);
        app.select_action(4); // Exit
        assert!(matches!(app.build_cli(), Outcome::Quit));
    }
}
