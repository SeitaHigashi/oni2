open Oni_Core;
open Oni_Core.Utility;

module Log = (val Log.withNamespace("Oni2.Feature.Explorer"));

// MODEL

include Model;

let getFileIcon = (~languageInfo, ~iconTheme, filePath) => {
  Exthost.LanguageInfo.getLanguageFromFilePath(languageInfo, filePath)
  |> IconTheme.getIconForFile(iconTheme, filePath);
};

module Internal = {
  let sortByLoweredDisplayName = (a: FsTreeNode.t, b: FsTreeNode.t) => {
    switch (a, b) {
    | (Node(_), Leaf(_)) => (-1)
    | (Leaf(_), Node(_)) => 1
    | (Node({data: a, _}), Node({data: b, _}))
    | (Leaf(a), Leaf(b)) =>
      compare(
        a.displayName |> String.lowercase_ascii,
        b.displayName |> String.lowercase_ascii,
      )
    };
  };

  let luvDirentToFsTree = (~getIcon, ~cwd, {name, kind}: Luv.File.Dirent.t) => {
    let path = Filename.concat(cwd, name);
    if (kind == `FILE || kind == `LINK) {
      Some(FsTreeNode.file(path, ~icon=getIcon(path)));
    } else if (kind == `DIR) {
      Some(FsTreeNode.directory(path, ~icon=getIcon(path), ~children=[]));
    } else {
      None;
    };
  };

  let luvDirentsToFsTree = (~getIcon, ~cwd, ~ignored, dirents) => {
    dirents
    |> List.filter(({name, _}: Luv.File.Dirent.t) =>
         name != ".." && name != "." && !List.mem(name, ignored)
       )
    |> List.filter_map(luvDirentToFsTree(~getIcon, ~cwd))
    |> List.sort(sortByLoweredDisplayName);
  };

  /**
    getFilesAndFolders

     This function uses Lwt to get all the files and folders in a directory
     then for each we check if it is a file or folder.
     if it is a directory we recursively call getFilesAndFolders on it
     to resolves its subfolders and files. We do this concurrently using
     Lwt_list.map_p. The recursion is gated by the depth value so it does
     not recurse too far.
   */
  let getFilesAndFolders = (~ignored, cwd, getIcon) => {
    cwd
    |> Service_OS.Api.readdir
    |> Lwt.map(luvDirentsToFsTree(~ignored, ~cwd, ~getIcon));
  };

  let getDirectoryTree = (cwd, languageInfo, iconTheme, ignored) => {
    let getIcon = getFileIcon(~languageInfo, ~iconTheme);
    let childrenPromise = getFilesAndFolders(~ignored, cwd, getIcon);

    childrenPromise
    |> Lwt.map(children => {
         FsTreeNode.directory(
           cwd,
           ~icon=getIcon(cwd),
           ~children,
           ~isOpen=true,
         )
       });
  };
};

module Effects = {
  let load = (directory, languageInfo, iconTheme, configuration, ~onComplete) => {
    Isolinear.Effect.createWithDispatch(~name="explorer.load", dispatch => {
      let ignored =
        Configuration.getValue(c => c.filesExclude, configuration);
      let promise =
        Internal.getDirectoryTree(
          directory,
          languageInfo,
          iconTheme,
          ignored,
        );

      Lwt.on_success(promise, tree => {dispatch(onComplete(tree))});
    });
  };
};

// UPDATE

type outmsg =
  | Nothing
  | Effect(Isolinear.Effect.t(msg))
  | OpenFile(string)
  | GrabFocus;

let setTree = (tree, model) => {

  let uniqueId = (data: FsTreeNode.metadata) => data.path;
  let treeView = Component_VimTree.set(~uniqueId, [tree], model.treeView);

{...model, tree: Some(tree), 
treeView,
};
};

let setActive = (maybePath, model) => {...model, active: maybePath};

let setFocus = (maybePath, model) =>
  switch (maybePath, model.tree) {
  | (Some(path), Some(tree)) =>
    switch (FsTreeNode.findByPath(path, tree)) {
    | Some(_) => {...model, focus: Some(path)}
    | None => model
    }
  | _ => {...model, focus: None}
  };
let setScrollOffset = (scrollOffset, model) => {...model, scrollOffset};

let replaceNode = (node, model: model) =>
  switch (model.tree) {
  | Some(tree) =>
    setTree(FsTreeNode.replace(~replacement=node, tree), model)
  | None => model
  };

let revealAndFocusPath =
    (~languageInfo, ~iconTheme, ~configuration, path, model: model) => {
  switch (model.tree) {
  | Some(tree) =>
    switch (FsTreeNode.findNodesByPath(path, tree)) {
    // Nothing to do
    | `Success([])
    | `Failed => (model, Nothing)

    // Load next unloaded node in path
    | `Partial(lastNode) => (
        model,
        Effect(
          Effects.load(
            FsTreeNode.getPath(lastNode),
            languageInfo,
            iconTheme,
            configuration,
            ~onComplete=node =>
            FocusNodeLoaded(node)
          ),
        ),
      )

    // Open ALL the nodes (in the path)!
    | `Success(_) =>
      let tree = FsTreeNode.updateNodesInPath(FsTreeNode.setOpen, path, tree);

      // TODO: Update scroll offset. We can use the view to find this!
      //      let offset =
      //        switch (FsTreeNode.expandedIndex(path, tree)) {
      //        | Some(offset) => `Middle(float(offset))
      //        | None => model.scrollOffset
      //        };
      let offset = `Start(0.);
      (
        model
        |> setFocus(Some(path))
        |> setTree(tree)
        |> setScrollOffset(offset),
        Nothing,
      );
    }

  | None => (model, Nothing)
  };
};

let revealFocus = model => {
  // TODO:
  model;
  //  switch (model.focus, model.tree) {
  //  | (Some(focus), Some(tree)) =>
  //    switch (FsTreeNode.expandedIndex(focus, tree)) {
  //    | Some(index) => {...model, scrollOffset: `Reveal(index)}
  //    | None => model
  //    }
  //  | _ => model
  //  };
};

let selectNode =
    (~languageInfo, ~iconTheme, ~configuration, node: FsTreeNode.t, model) =>
  // TODO:
  model;
//  switch (node) {
//  | {kind: File, path, _} =>
//    // Set active here to avoid scrolling in BufferEnter
//    (model |> setActive(Some(node.path)), OpenFile(path))
//
//  | {kind: Directory({isOpen, _}), _} => (
//      replaceNode(FsTreeNode.toggleOpen(node), model),
//      isOpen
//        ? Nothing
//        : Effect(
//            Effects.load(
//              node.path,
//              languageInfo,
//              iconTheme,
//              configuration,
//              ~onComplete=newNode =>
//              NodeLoaded(newNode)
//            ),
//          ),
//    )
//  };

let update = (~configuration, ~languageInfo, ~iconTheme, msg, model) => {
  switch (msg) {
  | ActiveFilePathChanged(maybeFilePath) =>
    switch (model) {
    | {active, _} when active != maybeFilePath =>
      switch (maybeFilePath) {
      | Some(path) =>
        let autoReveal =
          Oni_Core.Configuration.getValue(
            c => c.explorerAutoReveal,
            configuration,
          );
        switch (autoReveal) {
        | `HighlightAndScroll =>
          let model' = {...model, active: Some(path)};
          revealAndFocusPath(
            ~languageInfo,
            ~configuration,
            ~iconTheme,
            path,
            model',
          );
        | `HighlightOnly =>
          let model = setActive(Some(path), model);
          (setFocus(Some(path), model), Nothing);
        | `NoReveal => (model, Nothing)
        };
      | None => (model, Nothing)
      }
    | _ => (model, Nothing)
    }

  | TreeLoaded(tree) => (setTree(tree, model), Nothing)

  | TreeLoadError(_msg) => (model, Nothing)

  | NodeLoaded(node) => (replaceNode(node, model), Nothing)

  | FocusNodeLoaded(node) =>
    switch (model.active) {
    | Some(activePath) =>
      model
      |> replaceNode(node)
      |> revealAndFocusPath(
           ~languageInfo,
           ~configuration,
           ~iconTheme,
           activePath,
         )

    | None => (model, Nothing)
    }

  | KeyboardInput(_) =>
    // Anything to be brought back here?
    (model, Nothing)

  | NodeClicked(node) => (
      model
      |> setFocus(Some(FsTreeNode.getPath(node)))
      |> selectNode(~languageInfo, ~configuration, ~iconTheme, node),
      Nothing,
    )

  | ScrollOffsetChanged(offset) => (setScrollOffset(offset, model), Nothing)

  | Tree(treeMsg) =>
    let (treeView, outmsg) =
      Component_VimTree.update(treeMsg, model.treeView);

    let eff = switch (outmsg) {
    | Expanded(node) =>
        Effect(
            Effects.load(
              node.path,
              languageInfo,
              iconTheme,
              configuration,
              ~onComplete=newNode =>
              NodeLoaded(newNode)
            ),
          );
    | Collapsed(_) => Nothing
    | _ => Nothing
    };
    ({...model, treeView}, eff);
  };
};

module View = View;

let sub = (~configuration, ~languageInfo, ~iconTheme, {rootPath, _}) => {
  let getIcon = getFileIcon(~languageInfo, ~iconTheme);
  let ignored = Configuration.getValue(c => c.filesExclude, configuration);

  let toMsg =
    fun
    | Ok(dirents) => {
        let children =
          dirents
          |> Internal.luvDirentsToFsTree(~ignored, ~getIcon, ~cwd=rootPath);
        TreeLoaded(
          FsTreeNode.directory(
            rootPath,
            ~icon=getIcon(rootPath),
            ~children,
            ~isOpen=true,
          ),
        );
      }
    | Error(msg) => TreeLoadError(msg);

  Service_OS.Sub.dir(~uniqueId="FileExplorerSideBar", ~toMsg, rootPath);
};

module Contributions = {
  open WhenExpr.ContextKeys.Schema;

  let commands = (~isFocused) => {
    !isFocused
      ? []
      : 
        (
          Component_VimTree.Contributions.commands
          |> List.map(Oni_Core.Command.map(msg => Tree(msg)))
        );
  };

  let contextKeys = (~isFocused) => {
    let vimTreeKeys =
      isFocused ? Component_VimTree.Contributions.contextKeys : [];

    [
      vimTreeKeys |> fromList |> map(_ => ()),
    ]
    |> unionMany;
  };
};
